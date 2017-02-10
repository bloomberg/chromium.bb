// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/crnet/crnet_environment.h"

#import <Foundation/Foundation.h>

#include <utility>

#include "base/at_exit.h"
#include "base/atomicops.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/json/json_writer.h"
#include "base/mac/bind_objc_block.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_block.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "ios/crnet/sdch_owner_pref_storage.h"
#include "ios/net/cookies/cookie_store_ios.h"
#include "ios/net/crn_http_protocol_handler.h"
#include "ios/net/empty_nsurlcache.h"
#include "ios/net/http_cache_helper.h"
#include "ios/net/request_tracker.h"
#include "ios/web/public/user_agent.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/base/sdch_manager.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_known_logs.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_util.h"
#include "net/log/net_log.h"
#include "net/log/write_to_file_net_log_observer.h"
#include "net/net_features.h"
#include "net/proxy/proxy_service.h"
#include "net/sdch/sdch_owner.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/file_protocol_handler.h"
#include "net/url_request/sdch_dictionary_fetcher.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "url/url_features.h"
#include "url/url_util.h"

#if !BUILDFLAG(USE_PLATFORM_ICU_ALTERNATIVES)
#include "base/i18n/icu_util.h"  // nogncheck
#endif

namespace {

base::AtExitManager* g_at_exit_ = nullptr;

// Request context getter for CrNet.
class CrNetURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  CrNetURLRequestContextGetter(
      net::URLRequestContext* context,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
      : context_(context), task_runner_(task_runner) {}

  net::URLRequestContext* GetURLRequestContext() override { return context_; }

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override {
    return task_runner_;
  }
 private:
  // Must be called on the IO thread.
  ~CrNetURLRequestContextGetter() override {}

  net::URLRequestContext* context_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  DISALLOW_COPY_AND_ASSIGN(CrNetURLRequestContextGetter);
};

}  // namespace

// net::HTTPProtocolHandlerDelegate for CrNet.
class CrNetHttpProtocolHandlerDelegate
    : public net::HTTPProtocolHandlerDelegate {
 public:
  CrNetHttpProtocolHandlerDelegate(net::URLRequestContextGetter* getter,
                                   RequestFilterBlock filter)
      : getter_(getter), filter_(filter, base::scoped_policy::RETAIN) {}

 private:
  // net::HTTPProtocolHandlerDelegate implementation:
  bool CanHandleRequest(NSURLRequest* request) override {
    // Don't advertise support for file:// URLs for now.
    // This broke some time ago but it's not clear how to fix it at the moment.
    // http://crbug.com/480620
    if ([[[request URL] scheme] caseInsensitiveCompare:@"file"] ==
        NSOrderedSame) {
      return false;
    }
    if (filter_) {
      RequestFilterBlock block = filter_.get();
      return block(request);
    }
    return true;
  }

  bool IsRequestSupported(NSURLRequest* request) override {
    NSString* scheme = [[request URL] scheme];
    if (!scheme)
      return false;
    return [scheme caseInsensitiveCompare:@"data"] == NSOrderedSame ||
           [scheme caseInsensitiveCompare:@"http"] == NSOrderedSame ||
           [scheme caseInsensitiveCompare:@"https"] == NSOrderedSame;
  }

  net::URLRequestContextGetter* GetDefaultURLRequestContext() override {
    return getter_.get();
  }

  scoped_refptr<net::URLRequestContextGetter> getter_;
  base::mac::ScopedBlock<RequestFilterBlock> filter_;
};

void CrNetEnvironment::PostToNetworkThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  network_io_thread_->task_runner()->PostTask(from_here, task);
}

void CrNetEnvironment::PostToFileUserBlockingThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  file_user_blocking_thread_->task_runner()->PostTask(from_here, task);
}

// static
void CrNetEnvironment::Initialize() {
  DCHECK_EQ([NSThread currentThread], [NSThread mainThread]);
  if (!g_at_exit_)
    g_at_exit_ = new base::AtExitManager;

  // Change the framework bundle to the bundle that contain CrNet framework.
  // By default the framework bundle is set equal to the main (app) bundle.
  NSBundle* frameworkBundle = [NSBundle bundleForClass:CrNet.class];
  base::mac::SetOverrideFrameworkBundle(frameworkBundle);
#if !BUILDFLAG(USE_PLATFORM_ICU_ALTERNATIVES)
  CHECK(base::i18n::InitializeICU());
#endif
  url::Initialize();
  base::CommandLine::Init(0, nullptr);

  // Without doing this, StatisticsRecorder::FactoryGet() leaks one histogram
  // per call after the first for a given name.
  base::StatisticsRecorder::Initialize();

  // Create a message loop on the UI thread.
  base::MessageLoop* main_message_loop =
      new base::MessageLoop(base::MessageLoop::TYPE_UI);
#pragma unused(main_message_loop)
  base::MessageLoopForUI::current()->Attach();
}

void CrNetEnvironment::StartNetLog(base::FilePath::StringType file_name,
    bool log_bytes) {
  DCHECK(file_name.length());
  PostToFileUserBlockingThread(FROM_HERE,
      base::Bind(&CrNetEnvironment::StartNetLogInternal,
                 base::Unretained(this), file_name, log_bytes));
}

void CrNetEnvironment::StartNetLogInternal(
    base::FilePath::StringType file_name, bool log_bytes) {
  DCHECK(file_user_blocking_thread_->task_runner()->BelongsToCurrentThread());
  DCHECK(file_name.length());
  DCHECK(net_log_);

  if (net_log_observer_)
    return;

  base::FilePath temp_dir;
  if (!base::GetTempDir(&temp_dir))
    return;

  base::FilePath full_path = temp_dir.Append(file_name);
  base::ScopedFILE file(base::OpenFile(full_path, "w"));
  if (!file)
    return;

  net::NetLogCaptureMode capture_mode = log_bytes ?
      net::NetLogCaptureMode::IncludeSocketBytes() :
      net::NetLogCaptureMode::Default();

  net_log_observer_.reset(new net::WriteToFileNetLogObserver());
  net_log_observer_->set_capture_mode(capture_mode);
  net_log_observer_->StartObserving(net_log_.get(), std::move(file), nullptr,
                                    nullptr);
}

void CrNetEnvironment::StopNetLog() {
  PostToFileUserBlockingThread(FROM_HERE,
      base::Bind(&CrNetEnvironment::StopNetLogInternal,
      base::Unretained(this)));
}

void CrNetEnvironment::StopNetLogInternal() {
  DCHECK(file_user_blocking_thread_->task_runner()->BelongsToCurrentThread());
  if (net_log_observer_) {
    net_log_observer_->StopObserving(nullptr);
    net_log_observer_.reset();
  }
}

void CrNetEnvironment::CloseAllSpdySessions() {
  PostToNetworkThread(FROM_HERE,
      base::Bind(&CrNetEnvironment::CloseAllSpdySessionsInternal,
      base::Unretained(this)));
}

void CrNetEnvironment::SetRequestFilterBlock(RequestFilterBlock block) {
  http_protocol_handler_delegate_.reset(
      new CrNetHttpProtocolHandlerDelegate(main_context_getter_.get(), block));
  net::HTTPProtocolHandlerDelegate::SetInstance(
      http_protocol_handler_delegate_.get());
}

net::HttpNetworkSession* CrNetEnvironment::GetHttpNetworkSession(
    net::URLRequestContext* context) {
  DCHECK(context);
  if (!context->http_transaction_factory())
    return nullptr;

  return context->http_transaction_factory()->GetSession();
}

void CrNetEnvironment::CloseAllSpdySessionsInternal() {
  DCHECK(network_io_thread_->task_runner()->BelongsToCurrentThread());

  net::HttpNetworkSession* http_network_session =
      GetHttpNetworkSession(GetMainContextGetter()->GetURLRequestContext());

  if (http_network_session) {
    net::SpdySessionPool *spdy_session_pool =
        http_network_session->spdy_session_pool();
    if (spdy_session_pool)
      spdy_session_pool->CloseCurrentSessions(net::ERR_ABORTED);
  }
}

CrNetEnvironment::CrNetEnvironment(const std::string& user_agent,
                                   bool user_agent_partial)
    : spdy_enabled_(false),
      quic_enabled_(false),
      sdch_enabled_(false),
      main_context_(new net::URLRequestContext),
      user_agent_(user_agent),
      user_agent_partial_(user_agent_partial),
      net_log_(new net::NetLog) {}

void CrNetEnvironment::Install() {
  // Threads setup.
  network_cache_thread_.reset(new base::Thread("Chrome Network Cache Thread"));
  network_cache_thread_->StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
  network_io_thread_.reset(new base::Thread("Chrome Network IO Thread"));
  network_io_thread_->StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
  file_thread_.reset(new base::Thread("Chrome File Thread"));
  file_thread_->StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
  file_user_blocking_thread_.reset(
      new base::Thread("Chrome File User Blocking Thread"));
  file_user_blocking_thread_->StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));

  // The network change notifier must be initialized so that registered
  // delegates will receive callbacks.
  network_change_notifier_.reset(net::NetworkChangeNotifier::Create());
  proxy_config_service_ = net::ProxyService::CreateSystemProxyConfigService(
      network_io_thread_->task_runner(), nullptr);

  main_context_getter_ = new CrNetURLRequestContextGetter(
      main_context_.get(), network_io_thread_->task_runner());
  base::subtle::MemoryBarrier();
  PostToNetworkThread(FROM_HERE,
      base::Bind(&CrNetEnvironment::InitializeOnNetworkThread,
                 base::Unretained(this)));

  SetRequestFilterBlock(nil);
}

void CrNetEnvironment::InstallIntoSessionConfiguration(
    NSURLSessionConfiguration* config) {
  config.protocolClasses = @[ [CRNPauseableHTTPProtocolHandler class] ];
}

CrNetEnvironment::~CrNetEnvironment() {
  net::HTTPProtocolHandlerDelegate::SetInstance(nullptr);
}

net::URLRequestContextGetter* CrNetEnvironment::GetMainContextGetter() {
  return main_context_getter_.get();
}

void CrNetEnvironment::SetHTTPProtocolHandlerRegistered(bool registered) {
  if (registered) {
    // Disable the default cache.
    [NSURLCache setSharedURLCache:[EmptyNSURLCache emptyNSURLCache]];
    // Register the chrome http protocol handler to replace the default one.
    BOOL success =
        [NSURLProtocol registerClass:[CRNPauseableHTTPProtocolHandler class]];
    DCHECK(success);
  } else {
    // Set up an empty default cache, with default size.
    // TODO(droger): If the NSURLCache is to be used, its size should most
    // likely be changed. On an iPod2 with iOS4, the default size is 512k.
    [NSURLCache setSharedURLCache:[[[NSURLCache alloc] init] autorelease]];
    [NSURLProtocol unregisterClass:[CRNPauseableHTTPProtocolHandler class]];
  }
}

void CrNetEnvironment::ConfigureSdchOnNetworkThread() {
  DCHECK(network_io_thread_->task_runner()->BelongsToCurrentThread());
  net::URLRequestContext* context =
      main_context_getter_->GetURLRequestContext();

  if (!sdch_enabled_) {
    DCHECK_EQ(static_cast<net::SdchManager*>(nullptr), context->sdch_manager());
    return;
  }

  sdch_manager_.reset(new net::SdchManager());
  sdch_owner_.reset(new net::SdchOwner(sdch_manager_.get(), context));
  if (!sdch_pref_store_filename_.empty()) {
    base::FilePath path(sdch_pref_store_filename_);
    pref_store_worker_pool_ = file_user_blocking_thread_->task_runner();
    net_pref_store_ = new JsonPrefStore(path, pref_store_worker_pool_.get(),
                                        std::unique_ptr<PrefFilter>());
    net_pref_store_->ReadPrefsAsync(nullptr);
    sdch_owner_->EnablePersistentStorage(
        std::unique_ptr<net::SdchOwner::PrefStorage>(
            new SdchOwnerPrefStorage(net_pref_store_.get())));
  }
  context->set_sdch_manager(sdch_manager_.get());
}

void CrNetEnvironment::InitializeOnNetworkThread() {
  DCHECK(network_io_thread_->task_runner()->BelongsToCurrentThread());
  base::FeatureList::InitializeInstance(std::string(), std::string());

  ConfigureSdchOnNetworkThread();

  // Use the framework bundle to search for resources.
  NSBundle* frameworkBundle = base::mac::FrameworkBundle();
  NSString* bundlePath =
      [frameworkBundle pathForResource:@"crnet_resources" ofType:@"bundle"];
  NSBundle* bundle = [NSBundle bundleWithPath:bundlePath];
  NSString* acceptableLanguages = NSLocalizedStringWithDefaultValue(
      @"IDS_ACCEPT_LANGUAGES",
      @"Localizable",
      bundle,
      @"en-US,en",
      @"These values are copied from Chrome's .xtb files, so the same "
       "values are used in the |Accept-Language| header. Key name matches "
       "Chrome's.");
  if (acceptableLanguages == Nil)
    acceptableLanguages = @"en-US,en";
  std::string acceptable_languages =
      [acceptableLanguages cStringUsingEncoding:NSUTF8StringEncoding];
  if (user_agent_partial_) {
    user_agent_ = web::BuildUserAgentFromProduct(user_agent_);
    user_agent_partial_ = false;
  }
  // Set the user agent through NSUserDefaults. This sets it for both
  // UIWebViews and WKWebViews, and javascript calls to navigator.userAgent
  // return this value.
  [[NSUserDefaults standardUserDefaults] registerDefaults:@{
    @"UserAgent" : [NSString stringWithUTF8String:user_agent_.c_str()]
  }];
  main_context_->set_http_user_agent_settings(
      new net::StaticHttpUserAgentSettings(acceptable_languages, user_agent_));

  main_context_->set_ssl_config_service(new net::SSLConfigServiceDefaults);
  main_context_->set_transport_security_state(
      new net::TransportSecurityState());
  std::unique_ptr<net::MultiLogCTVerifier> ct_verifier =
      base::MakeUnique<net::MultiLogCTVerifier>();
  ct_verifier->AddLogs(net::ct::CreateLogVerifiersForKnownLogs());
  // TODO(mef): Note that the ".release()" calls below are leaking
  // the objects in question; this should be fixed by having an object
  // corresponding to URLRequestContextStorage that actually owns those
  // objects.  See http://crbug.com/523858.
  main_context_->set_cert_transparency_verifier(ct_verifier.release());
  main_context_->set_ct_policy_enforcer(new net::CTPolicyEnforcer());
  http_server_properties_.reset(new net::HttpServerPropertiesImpl());
  main_context_->set_http_server_properties(http_server_properties_.get());
  main_context_->set_host_resolver(
      net::HostResolver::CreateDefaultResolver(nullptr).release());
  main_context_->set_cert_verifier(
      net::CertVerifier::CreateDefault().release());
  main_context_->set_http_auth_handler_factory(
      net::HttpAuthHandlerRegistryFactory::CreateDefault(
          main_context_->host_resolver())
          .release());
  main_context_->set_proxy_service(
      net::ProxyService::CreateUsingSystemProxyResolver(
          std::move(proxy_config_service_), 0, nullptr)
          .release());

  // Cache
  NSArray* dirs = NSSearchPathForDirectoriesInDomains(NSCachesDirectory,
                                                      NSUserDomainMask,
                                                      YES);
  base::FilePath cache_path =
      base::mac::NSStringToFilePath([dirs objectAtIndex:0]);
  cache_path = cache_path.Append(FILE_PATH_LITERAL("crnet"));
  std::unique_ptr<net::HttpCache::DefaultBackend> main_backend(
      new net::HttpCache::DefaultBackend(net::DISK_CACHE,
                                         net::CACHE_BACKEND_DEFAULT, cache_path,
                                         0,  // Default cache size.
                                         network_cache_thread_->task_runner()));

  net::HttpNetworkSession::Params params;
  params.host_resolver = main_context_->host_resolver();
  params.cert_verifier = main_context_->cert_verifier();
  params.cert_transparency_verifier =
      main_context_->cert_transparency_verifier();
  params.ct_policy_enforcer = main_context_->ct_policy_enforcer();
  params.channel_id_service = main_context_->channel_id_service();
  params.transport_security_state = main_context_->transport_security_state();
  params.proxy_service = main_context_->proxy_service();
  params.ssl_config_service = main_context_->ssl_config_service();
  params.http_auth_handler_factory = main_context_->http_auth_handler_factory();
  params.http_server_properties = main_context_->http_server_properties();
  params.net_log = main_context_->net_log();
  params.enable_http2 = spdy_enabled();
  params.enable_quic = quic_enabled();

  if (!params.channel_id_service) {
    // The main context may not have a ChannelIDService, since it is lazily
    // constructed. If not, build an ephemeral ChannelIDService with no backing
    // disk store.
    // TODO(ellyjones): support persisting ChannelID.
    params.channel_id_service =
        new net::ChannelIDService(new net::DefaultChannelIDStore(NULL));
  }

  // TODO(mmenke):  These really shouldn't be leaked.
  //                See https://crbug.com/523858.
  net::HttpNetworkSession* http_network_session =
      new net::HttpNetworkSession(params);
  net::HttpCache* main_cache =
      new net::HttpCache(http_network_session, std::move(main_backend),
                         true /* set_up_quic_server_info */);
  main_context_->set_http_transaction_factory(main_cache);

  // Cookies
  [[NSHTTPCookieStorage sharedHTTPCookieStorage]
      setCookieAcceptPolicy:NSHTTPCookieAcceptPolicyAlways];
  cookie_store_ = base::MakeUnique<net::CookieStoreIOS>(
      [NSHTTPCookieStorage sharedHTTPCookieStorage]);
  main_context_->set_cookie_store(cookie_store_.get());

  net::URLRequestJobFactoryImpl* job_factory =
      new net::URLRequestJobFactoryImpl;
  job_factory->SetProtocolHandler(
      "data", base::WrapUnique(new net::DataProtocolHandler));
#if !BUILDFLAG(DISABLE_FILE_SUPPORT)
  job_factory->SetProtocolHandler(
      "file",
      base::MakeUnique<net::FileProtocolHandler>(file_thread_->task_runner()));
#endif   // !BUILDFLAG(DISABLE_FILE_SUPPORT)
  main_context_->set_job_factory(job_factory);

  main_context_->set_net_log(net_log_.get());
}

std::string CrNetEnvironment::user_agent() {
  const net::HttpUserAgentSettings* user_agent_settings =
      main_context_->http_user_agent_settings();
  if (!user_agent_settings) {
    return nullptr;
  }

  return user_agent_settings->GetUserAgent();
}

void CrNetEnvironment::ClearCache(ClearCacheCallback callback) {
  PostToNetworkThread(
      FROM_HERE, base::Bind(&net::ClearHttpCache, main_context_getter_,
                            network_io_thread_->task_runner(), base::Time(),
                            base::Time::Max(), base::BindBlock(callback)));
}

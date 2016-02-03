// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/crnet/crnet_environment.h"

#import <Foundation/Foundation.h>

#include <utility>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/i18n/icu_util.h"
#include "base/json/json_writer.h"
#include "base/mac/bind_objc_block.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_block.h"
#include "base/macros.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/threading/worker_pool.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#import "components/webp_transcode/webp_network_client_factory.h"
#include "crypto/nss_util.h"
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
#include "net/cert_net/nss_ocsp.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_util.h"
#include "net/log/net_log.h"
#include "net/log/write_to_file_net_log_observer.h"
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
#include "url/url_util.h"

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
  network_io_thread_->message_loop()->PostTask(from_here, task);
}

void CrNetEnvironment::PostToFileUserBlockingThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  file_user_blocking_thread_->message_loop()->PostTask(from_here, task);
}

// static
void CrNetEnvironment::Initialize() {
  DCHECK_EQ([NSThread currentThread], [NSThread mainThread]);
  if (!g_at_exit_)
    g_at_exit_ = new base::AtExitManager;

  CHECK(base::i18n::InitializeICU());
  url::Initialize();
  base::CommandLine::Init(0, nullptr);
  // This needs to happen on the main thread. NSPR's initialization sets up its
  // memory allocator; if this is not done before other threads are created,
  // this initialization can race to cause accidental free/allocation
  // mismatches.
  crypto::EnsureNSPRInit();
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
  DCHECK(base::MessageLoop::current() ==
         file_user_blocking_thread_->message_loop());
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
  DCHECK(base::MessageLoop::current() ==
         file_user_blocking_thread_->message_loop());
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
  DCHECK(base::MessageLoop::current() ==
         network_io_thread_->message_loop());

  net::HttpNetworkSession* http_network_session =
      GetHttpNetworkSession(GetMainContextGetter()->GetURLRequestContext());

  if (http_network_session) {
    net::SpdySessionPool *spdy_session_pool =
        http_network_session->spdy_session_pool();
    if (spdy_session_pool)
      spdy_session_pool->CloseCurrentSessions(net::ERR_ABORTED);
  }
}

CrNetEnvironment::CrNetEnvironment(const std::string& user_agent_product_name)
    : spdy_enabled_(false),
      quic_enabled_(false),
      sdch_enabled_(false),
      main_context_(new net::URLRequestContext),
      user_agent_product_name_(user_agent_product_name),
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

  PostToNetworkThread(FROM_HERE,
      base::Bind(&CrNetEnvironment::InitializeOnNetworkThread,
                 base::Unretained(this)));

  net::SetURLRequestContextForNSSHttpIO(main_context_.get());
  main_context_getter_ = new CrNetURLRequestContextGetter(
      main_context_.get(), network_io_thread_->task_runner());
  SetRequestFilterBlock(nil);
}

void CrNetEnvironment::InstallIntoSessionConfiguration(
    NSURLSessionConfiguration* config) {
  config.protocolClasses = @[ [CRNPauseableHTTPProtocolHandler class] ];
}

CrNetEnvironment::~CrNetEnvironment() {
  net::HTTPProtocolHandlerDelegate::SetInstance(nullptr);
  net::SetURLRequestContextForNSSHttpIO(nullptr);
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
  DCHECK(base::MessageLoop::current() == network_io_thread_->message_loop());
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
    net_pref_store_ = new JsonPrefStore(
        path,
        pref_store_worker_pool_.get(),
        scoped_ptr<PrefFilter>());
    net_pref_store_->ReadPrefsAsync(nullptr);
    sdch_owner_->EnablePersistentStorage(
        scoped_ptr<net::SdchOwner::PrefStorage>(
            new SdchOwnerPrefStorage(net_pref_store_.get())));
  }
  context->set_sdch_manager(sdch_manager_.get());
}

void CrNetEnvironment::InitializeOnNetworkThread() {
  DCHECK(base::MessageLoop::current() == network_io_thread_->message_loop());

  // Register network clients.
  net::RequestTracker::AddGlobalNetworkClientFactory(
      [[[WebPNetworkClientFactory alloc]
          initWithTaskRunner:file_user_blocking_thread_
                                 ->task_runner()] autorelease]);

  ConfigureSdchOnNetworkThread();

  NSString* bundlePath =
      [[NSBundle mainBundle] pathForResource:@"crnet_resources"
                                      ofType:@"bundle"];
  NSBundle* bundle = [NSBundle bundleWithPath:bundlePath];
  NSString* acceptableLanguages = NSLocalizedStringWithDefaultValue(
      @"IDS_ACCEPT_LANGUAGES",
      @"Localizable",
      bundle,
      @"en-US,en",
      @"These values are copied from Chrome's .xtb files, so the same "
       "values are used in the |Accept-Language| header. Key name matches "
       "Chrome's.");
  DCHECK(acceptableLanguages);
  std::string acceptable_languages =
      [acceptableLanguages cStringUsingEncoding:NSUTF8StringEncoding];
  std::string user_agent =
      web::BuildUserAgentFromProduct(user_agent_product_name_);
  // Set the user agent through NSUserDefaults. This sets it for both
  // UIWebViews and WKWebViews, and javascript calls to navigator.userAgent
  // return this value.
  [[NSUserDefaults standardUserDefaults] registerDefaults:@{
    @"UserAgent" : [NSString stringWithUTF8String:user_agent.c_str()]
  }];
  main_context_->set_http_user_agent_settings(
      new net::StaticHttpUserAgentSettings(acceptable_languages, user_agent));

  main_context_->set_ssl_config_service(new net::SSLConfigServiceDefaults);
  main_context_->set_transport_security_state(
      new net::TransportSecurityState());
  http_server_properties_.reset(new net::HttpServerPropertiesImpl());
  main_context_->set_http_server_properties(
      http_server_properties_->GetWeakPtr());
  // TODO(rdsmith): Note that the ".release()" calls below are leaking
  // the objects in question; this should be fixed by having an object
  // corresponding to URLRequestContextStorage that actually owns those
  // objects.  See http://crbug.com/523858.
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
  scoped_ptr<net::HttpCache::DefaultBackend> main_backend(
      new net::HttpCache::DefaultBackend(net::DISK_CACHE,
                                         net::CACHE_BACKEND_DEFAULT, cache_path,
                                         0,  // Default cache size.
                                         network_cache_thread_->task_runner()));

  net::HttpNetworkSession::Params params;
  params.host_resolver = main_context_->host_resolver();
  params.cert_verifier = main_context_->cert_verifier();
  params.channel_id_service = main_context_->channel_id_service();
  params.transport_security_state = main_context_->transport_security_state();
  params.proxy_service = main_context_->proxy_service();
  params.ssl_config_service = main_context_->ssl_config_service();
  params.http_auth_handler_factory = main_context_->http_auth_handler_factory();
  params.network_delegate = main_context_->network_delegate();
  params.http_server_properties = main_context_->http_server_properties();
  params.net_log = main_context_->net_log();
  params.enable_spdy31 = spdy_enabled();
  params.enable_http2 = spdy_enabled();
  params.parse_alternative_services = false;
  params.enable_quic = quic_enabled();
  params.alternative_service_probability_threshold =
      alternate_protocol_threshold_;

  if (!params.channel_id_service) {
    // The main context may not have a ChannelIDService, since it is lazily
    // constructed. If not, build an ephemeral ChannelIDService with no backing
    // disk store.
    // TODO(ellyjones): support persisting ChannelID.
    params.channel_id_service = new net::ChannelIDService(
        new net::DefaultChannelIDStore(NULL),
        base::WorkerPool::GetTaskRunner(true));
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
  scoped_refptr<net::CookieStore> cookie_store =
  net::CookieStoreIOS::CreateCookieStore(
      [NSHTTPCookieStorage sharedHTTPCookieStorage]);
  main_context_->set_cookie_store(cookie_store.get());

  net::URLRequestJobFactoryImpl* job_factory =
      new net::URLRequestJobFactoryImpl;
  job_factory->SetProtocolHandler(
      "data", make_scoped_ptr(new net::DataProtocolHandler));
  job_factory->SetProtocolHandler(
      "file", make_scoped_ptr(
                  new net::FileProtocolHandler(file_thread_->task_runner())));
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
      FROM_HERE,
      base::Bind(&net::ClearHttpCache, main_context_getter_,
                 network_io_thread_->task_runner(), base::BindBlock(callback)));
}

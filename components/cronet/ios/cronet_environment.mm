// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/ios/cronet_environment.h"

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
#include "base/mac/foundation_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_scheduler/task_scheduler.h"
#include "components/cronet/histogram_manager.h"
#include "components/cronet/ios/version.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "ios/net/cookies/cookie_store_ios.h"
#include "ios/web/public/user_agent.h"
#include "net/base/network_change_notifier.h"
#include "net/cert/cert_verifier.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/write_to_file_net_log_observer.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/ssl_client_socket.h"
#include "net/ssl/channel_id_service.h"
#include "net/url_request/http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "url/scheme_host_port.h"
#include "url/url_util.h"

namespace {

base::AtExitManager* g_at_exit_ = nullptr;
net::NetworkChangeNotifier* g_network_change_notifier = nullptr;
// MessageLoop on the main thread.
base::MessageLoop* g_main_message_loop = nullptr;

// Request context getter for Cronet.
class CronetURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  CronetURLRequestContextGetter(
      cronet::CronetEnvironment* environment,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
      : environment_(environment), task_runner_(task_runner) {}

  net::URLRequestContext* GetURLRequestContext() override {
    DCHECK(environment_);
    return environment_->GetURLRequestContext();
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override {
    return task_runner_;
  }

 private:
  // Must be called on the IO thread.
  ~CronetURLRequestContextGetter() override {}

  cronet::CronetEnvironment* environment_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  DISALLOW_COPY_AND_ASSIGN(CronetURLRequestContextGetter);
};

}  // namespace

namespace cronet {

void CronetEnvironment::PostToNetworkThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  network_io_thread_->task_runner()->PostTask(from_here, task);
}

void CronetEnvironment::PostToFileUserBlockingThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  file_user_blocking_thread_->task_runner()->PostTask(from_here, task);
}

net::URLRequestContext* CronetEnvironment::GetURLRequestContext() const {
  return main_context_.get();
}

net::URLRequestContextGetter* CronetEnvironment::GetURLRequestContextGetter()
    const {
  return main_context_getter_.get();
}

// static
void CronetEnvironment::Initialize() {
  // This method must be called once from the main thread.
  DCHECK_EQ([NSThread currentThread], [NSThread mainThread]);
  if (!g_at_exit_)
    g_at_exit_ = new base::AtExitManager;

  base::TaskScheduler::CreateAndStartWithDefaultParams("CronetIos");

  url::Initialize();
  base::CommandLine::Init(0, nullptr);

  // Without doing this, StatisticsRecorder::FactoryGet() leaks one histogram
  // per call after the first for a given name.
  base::StatisticsRecorder::Initialize();

  // Create a message loop on the UI thread.
  DCHECK(!base::MessageLoop::current());
  DCHECK(!g_main_message_loop);
  g_main_message_loop = new base::MessageLoopForUI();
  base::MessageLoopForUI::current()->Attach();
  // The network change notifier must be initialized so that registered
  // delegates will receive callbacks.
  DCHECK(!g_network_change_notifier);
  g_network_change_notifier = net::NetworkChangeNotifier::Create();
}

bool CronetEnvironment::StartNetLog(base::FilePath::StringType file_name,
                                    bool log_bytes) {
  if (!file_name.length())
    return false;

  base::FilePath path(file_name);

  base::ScopedFILE file(base::OpenFile(path, "w"));
  if (!file) {
    LOG(ERROR) << "Can not start NetLog to " << path.value() << ": "
               << strerror(errno);
    return false;
  }

  LOG(WARNING) << "Starting NetLog to " << path.value();
  PostToNetworkThread(
      FROM_HERE,
      base::Bind(&CronetEnvironment::StartNetLogOnNetworkThread,
                 base::Unretained(this), base::Passed(&file), log_bytes));

  return true;
}

void CronetEnvironment::StartNetLogOnNetworkThread(base::ScopedFILE file,
                                                   bool log_bytes) {
  DCHECK(net_log_);

  if (net_log_observer_)
    return;

  net::NetLogCaptureMode capture_mode =
      log_bytes ? net::NetLogCaptureMode::IncludeSocketBytes()
                : net::NetLogCaptureMode::Default();

  net_log_observer_.reset(new net::WriteToFileNetLogObserver());
  net_log_observer_->set_capture_mode(capture_mode);
  net_log_observer_->StartObserving(main_context_->net_log(), std::move(file),
                                    nullptr, main_context_.get());
  LOG(WARNING) << "Started NetLog";
}

void CronetEnvironment::StopNetLog() {
  base::WaitableEvent log_stopped_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  PostToNetworkThread(FROM_HERE,
                      base::Bind(&CronetEnvironment::StopNetLogOnNetworkThread,
                                 base::Unretained(this), &log_stopped_event));
  log_stopped_event.Wait();
}

void CronetEnvironment::StopNetLogOnNetworkThread(
    base::WaitableEvent* log_stopped_event) {
  if (net_log_observer_) {
    DLOG(WARNING) << "Stopped NetLog.";
    net_log_observer_->StopObserving(main_context_.get());
    net_log_observer_.reset();
  }
  log_stopped_event->Signal();
}

net::HttpNetworkSession* CronetEnvironment::GetHttpNetworkSession(
    net::URLRequestContext* context) {
  DCHECK(context);
  if (!context->http_transaction_factory())
    return nullptr;

  return context->http_transaction_factory()->GetSession();
}

void CronetEnvironment::AddQuicHint(const std::string& host,
                                    int port,
                                    int alternate_port) {
  DCHECK(port == alternate_port);
  quic_hints_.push_back(net::HostPortPair(host, port));
}

CronetEnvironment::CronetEnvironment(const std::string& user_agent,
                                     bool user_agent_partial)
    : http2_enabled_(false),
      quic_enabled_(false),
      http_cache_(URLRequestContextConfig::HttpCacheType::DISK),
      user_agent_(user_agent),
      user_agent_partial_(user_agent_partial),
      net_log_(new net::NetLog) {}

void CronetEnvironment::Start() {
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

  main_context_getter_ = new CronetURLRequestContextGetter(
      this, network_io_thread_->task_runner());
  base::subtle::MemoryBarrier();
  PostToNetworkThread(FROM_HERE,
                      base::Bind(&CronetEnvironment::InitializeOnNetworkThread,
                                 base::Unretained(this)));
}

CronetEnvironment::~CronetEnvironment() {
  // TODO(lilyhoughton) make unregistering of this work.
  // net::HTTPProtocolHandlerDelegate::SetInstance(nullptr);

  // TODO(lilyhoughton) this can only be run once, so right now leaking it.
  // Should be be called when the _last_ CronetEnvironment is destroyed.
  // base::TaskScheduler* ts = base::TaskScheduler::GetInstance();
  // if (ts)
  //  ts->Shutdown();

  // TODO(lilyhoughton) this should be smarter about making sure there are no
  // pending requests, etc.

  network_io_thread_->task_runner().get()->DeleteSoon(FROM_HERE,
                                                      main_context_.release());
}

void CronetEnvironment::InitializeOnNetworkThread() {
  DCHECK(network_io_thread_->task_runner()->BelongsToCurrentThread());
  base::FeatureList::InitializeInstance(std::string(), std::string());

  static bool ssl_key_log_file_set = false;
  if (!ssl_key_log_file_set && !ssl_key_log_file_name_.empty()) {
    ssl_key_log_file_set = true;
    base::FilePath ssl_key_log_file(ssl_key_log_file_name_);
    net::SSLClientSocket::SetSSLKeyLogFile(ssl_key_log_file,
                                           file_thread_->task_runner());
  }

  if (user_agent_partial_)
    user_agent_ = web::BuildUserAgentFromProduct(user_agent_);

  // Cache
  base::FilePath cache_path;
  if (!PathService::Get(base::DIR_CACHE, &cache_path))
    return;
  cache_path = cache_path.Append(FILE_PATH_LITERAL("cronet"));

  URLRequestContextConfigBuilder context_config_builder;
  context_config_builder.enable_quic = quic_enabled_;   // Enable QUIC.
  context_config_builder.enable_spdy = http2_enabled_;  // Enable HTTP/2.
  context_config_builder.http_cache = http_cache_;      // Set HTTP cache
  context_config_builder.storage_path =
      cache_path.value();  // Storage path for http cache and cookie storage.
  context_config_builder.user_agent =
      user_agent_;  // User-Agent request header field.
  context_config_builder.experimental_options =
      experimental_options_;  // Set experimental Cronet options.
  context_config_builder.mock_cert_verifier = std::move(
      mock_cert_verifier_);  // MockCertVerifier to use for testing purposes.
  std::unique_ptr<URLRequestContextConfig> config =
      context_config_builder.Build();

  config->pkp_list = std::move(pkp_list_);

  net::URLRequestContextBuilder context_builder;

  context_builder.set_accept_language(accept_language_);

  // Explicitly disable the persister for Cronet to avoid persistence of dynamic
  // HPKP.  This is a safety measure ensuring that nobody enables the
  // persistence of HPKP by specifying transport_security_persister_path in the
  // future.
  context_builder.set_transport_security_persister_path(base::FilePath());

  config->ConfigureURLRequestContextBuilder(&context_builder, net_log_.get(),
                                            file_thread_.get()->task_runner());

  std::unique_ptr<net::MappedHostResolver> mapped_host_resolver(
      new net::MappedHostResolver(
          net::HostResolver::CreateDefaultResolver(nullptr)));

  context_builder.set_host_resolver(std::move(mapped_host_resolver));

  // TODO(690969): This behavior matches previous behavior of CookieStoreIOS in
  // CrNet, but should change to adhere to App's Cookie Accept Policy instead
  // of changing it.
  [[NSHTTPCookieStorage sharedHTTPCookieStorage]
      setCookieAcceptPolicy:NSHTTPCookieAcceptPolicyAlways];
  std::unique_ptr<net::CookieStore> cookie_store =
      base::MakeUnique<net::CookieStoreIOS>(
          [NSHTTPCookieStorage sharedHTTPCookieStorage]);
  context_builder.SetCookieAndChannelIdStores(std::move(cookie_store), nullptr);

  std::unique_ptr<net::HttpServerProperties> http_server_properties(
      new net::HttpServerPropertiesImpl());
  for (const auto& quic_hint : quic_hints_) {
    net::AlternativeService alternative_service(net::kProtoQUIC, "",
                                                quic_hint.port());
    url::SchemeHostPort quic_hint_server("https", quic_hint.host(),
                                         quic_hint.port());
    http_server_properties->SetAlternativeService(
        quic_hint_server, alternative_service, base::Time::Max());
  }

  context_builder.SetHttpServerProperties(std::move(http_server_properties));

  main_context_ = context_builder.Build();

  // Iterate through PKP configuration for every host.
  for (const auto& pkp : config->pkp_list) {
    // Add the host pinning.
    main_context_->transport_security_state()->AddHPKP(
        pkp->host, pkp->expiration_date, pkp->include_subdomains,
        pkp->pin_hashes, GURL::EmptyGURL());
  }
}

std::string CronetEnvironment::user_agent() {
  const net::HttpUserAgentSettings* user_agent_settings =
      main_context_->http_user_agent_settings();
  if (!user_agent_settings) {
    return nullptr;
  }

  return user_agent_settings->GetUserAgent();
}

std::vector<uint8_t> CronetEnvironment::GetHistogramDeltas() {
  DCHECK(base::StatisticsRecorder::IsActive());
  std::vector<uint8_t> data;
  if (!HistogramManager::GetInstance()->GetDeltas(&data))
    return std::vector<uint8_t>();
  return data;
}

void CronetEnvironment::SetHostResolverRules(const std::string& rules) {
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  PostToNetworkThread(
      FROM_HERE,
      base::Bind(&CronetEnvironment::SetHostResolverRulesOnNetworkThread,
                 base::Unretained(this), rules, &event));
  event.Wait();
}

void CronetEnvironment::SetHostResolverRulesOnNetworkThread(
    const std::string& rules,
    base::WaitableEvent* event) {
  static_cast<net::MappedHostResolver*>(main_context_->host_resolver())
      ->SetRulesFromString(rules);
  event->Signal();
}

std::string CronetEnvironment::getDefaultQuicUserAgentId() const {
  return base::SysNSStringToUTF8([[NSBundle mainBundle]
             objectForInfoDictionaryKey:@"CFBundleDisplayName"]) +
         " Cronet/" + CRONET_VERSION;
}

}  // namespace cronet

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/io_thread.h"

#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/leak_tracker.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread.h"
#include "base/threading/worker_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/data_usage/tab_id_annotator.h"
#include "chrome/browser/data_use_measurement/chrome_data_use_ascriber.h"
#include "chrome/browser/net/async_dns_field_trial.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/dns_probe_service.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/browser/net/sth_distributor_provider.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/certificate_transparency/tree_state_tracker.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/data_usage/core/data_use_aggregator.h"
#include "components/data_usage/core/data_use_amortizer.h"
#include "components/data_usage/core/data_use_annotator.h"
#include "components/data_use_measurement/core/data_use_ascriber.h"
#include "components/metrics/metrics_service.h"
#include "components/net_log/chrome_net_log.h"
#include "components/network_session_configurator/network_session_configurator.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "extensions/features/features.h"
#include "net/base/host_mapping_rules.h"
#include "net/base/logging_network_change_observer.h"
#include "net/base/sdch_manager.h"
#include "net/cert/caching_cert_verifier.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/ct_known_logs.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cert/multi_threaded_cert_verifier.h"
#include "net/cert/sth_distributor.h"
#include "net/cert/sth_observer.h"
#include "net/cookies/cookie_store.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/http/http_auth_filter.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_auth_preferences.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_server_properties_impl.h"
#include "net/net_features.h"
#include "net/nqe/external_estimate_provider.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_script_fetcher_impl.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/file_protocol_handler.h"
#include "net/url_request/ftp_protocol_handler.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/event_router_forwarder.h"
#endif

#if defined(USE_NSS_CERTS)
#include "net/cert_net/nss_ocsp.h"
#endif

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "base/android/build_info.h"
#include "chrome/browser/android/data_usage/external_data_use_observer.h"
#include "chrome/browser/android/net/external_estimate_provider_android.h"
#include "components/data_usage/android/traffic_stats_amortizer.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/net/cert_verify_proc_chromeos.h"
#include "chromeos/network/host_resolver_impl_chromeos.h"
#endif

#if defined(OS_ANDROID) && defined(ARCH_CPU_ARMEL)
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/cpu.h"
#endif

using content::BrowserThread;

class SafeBrowsingURLRequestContext;

// The IOThread object must outlive any tasks posted to the IO thread before the
// Quit task, so base::Bind() calls are not refcounted.

namespace {

// Field trial for network quality estimator. Seeds RTT and downstream
// throughput observations with values that correspond to the connection type
// determined by the operating system.
const char kNetworkQualityEstimatorFieldTrialName[] = "NetworkQualityEstimator";

#if defined(OS_MACOSX)
void ObserveKeychainEvents() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  net::CertDatabase::GetInstance()->SetMessageLoopForKeychainEvents();
}
#endif

// Gets file path into ssl_keylog_file from command line argument or
// environment variable. Command line argument has priority when
// both specified.
base::FilePath GetSSLKeyLogFile(const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kSSLKeyLogFile)) {
    base::FilePath path =
        command_line.GetSwitchValuePath(switches::kSSLKeyLogFile);
    if (!path.empty())
      return path;
    LOG(WARNING) << "ssl-key-log-file argument missing";
  }

  std::unique_ptr<base::Environment> env(base::Environment::Create());
  std::string path_str;
  env->GetVar("SSLKEYLOGFILE", &path_str);
#if defined(OS_WIN)
  // base::Environment returns environment variables in UTF-8 on Windows.
  return base::FilePath(base::UTF8ToUTF16(path_str));
#else
  return base::FilePath(path_str);
#endif
}

// Used for the "system" URLRequestContext.
class SystemURLRequestContext : public net::URLRequestContext {
 public:
  SystemURLRequestContext() {
#if defined(USE_NSS_CERTS)
    net::SetURLRequestContextForNSSHttpIO(this);
#endif
  }

 private:
  ~SystemURLRequestContext() override {
    AssertNoURLRequests();
#if defined(USE_NSS_CERTS)
    net::SetURLRequestContextForNSSHttpIO(NULL);
#endif
  }
};

std::unique_ptr<net::HostResolver> CreateGlobalHostResolver(
    net::NetLog* net_log) {
  TRACE_EVENT0("startup", "IOThread::CreateGlobalHostResolver");
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  net::HostResolver::Options options;

  // Use the retry attempts override from the command-line, if any.
  if (command_line.HasSwitch(switches::kHostResolverRetryAttempts)) {
    std::string s =
        command_line.GetSwitchValueASCII(switches::kHostResolverRetryAttempts);
    // Parse the switch (it should be a non-negative integer).
    int n;
    if (base::StringToInt(s, &n) && n >= 0) {
      options.max_retry_attempts = static_cast<size_t>(n);
    } else {
      LOG(ERROR) << "Invalid switch for host resolver retry attempts: " << s;
    }
  }

  std::unique_ptr<net::HostResolver> global_host_resolver;
#if defined OS_CHROMEOS
  global_host_resolver =
      chromeos::HostResolverImplChromeOS::CreateSystemResolver(options,
                                                               net_log);
#else
  global_host_resolver =
      net::HostResolver::CreateSystemResolver(options, net_log);
#endif

  // If hostname remappings were specified on the command-line, layer these
  // rules on top of the real host resolver. This allows forwarding all requests
  // through a designated test server.
  if (!command_line.HasSwitch(switches::kHostResolverRules))
    return global_host_resolver;

  std::unique_ptr<net::MappedHostResolver> remapped_resolver(
      new net::MappedHostResolver(std::move(global_host_resolver)));
  remapped_resolver->SetRulesFromString(
      command_line.GetSwitchValueASCII(switches::kHostResolverRules));
  return std::move(remapped_resolver);
}

int GetSwitchValueAsInt(const base::CommandLine& command_line,
                        const std::string& switch_name) {
  int value;
  if (!base::StringToInt(command_line.GetSwitchValueASCII(switch_name),
                         &value)) {
    return 0;
  }
  return value;
}

// This function is for forwarding metrics usage pref changes to the metrics
// service on the appropriate thread.
// TODO(gayane): Reduce the frequency of posting tasks from IO to UI thread.
void UpdateMetricsUsagePrefsOnUIThread(const std::string& service_name,
                                       int message_size,
                                       bool is_cellular) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind([](const std::string& service_name,
                    int message_size,
                    bool is_cellular) {
                   // Some unit tests use IOThread but do not initialize
                   // MetricsService. In that case it's fine to skip the update.
                   auto metrics_service = g_browser_process->metrics_service();
                   if (metrics_service) {
                     metrics_service->UpdateMetricsUsagePrefs(service_name,
                                                              message_size,
                                                              is_cellular);
                   }
                 },
                 service_name,
                 message_size,
                 is_cellular));
}

}  // namespace

class SystemURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  explicit SystemURLRequestContextGetter(IOThread* io_thread);

  // Implementation for net::UrlRequestContextGetter.
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

 protected:
  ~SystemURLRequestContextGetter() override;

 private:
  IOThread* const io_thread_;  // Weak pointer, owned by BrowserProcess.
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  base::debug::LeakTracker<SystemURLRequestContextGetter> leak_tracker_;
};

SystemURLRequestContextGetter::SystemURLRequestContextGetter(
    IOThread* io_thread)
    : io_thread_(io_thread),
      network_task_runner_(
          BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)) {}

SystemURLRequestContextGetter::~SystemURLRequestContextGetter() {}

net::URLRequestContext* SystemURLRequestContextGetter::GetURLRequestContext() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(io_thread_->globals()->system_request_context.get());

  return io_thread_->globals()->system_request_context.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
SystemURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

IOThread::Globals::
SystemRequestContextLeakChecker::SystemRequestContextLeakChecker(
    Globals* globals)
    : globals_(globals) {
  DCHECK(globals_);
}

IOThread::Globals::
SystemRequestContextLeakChecker::~SystemRequestContextLeakChecker() {
  if (globals_->system_request_context.get())
    globals_->system_request_context->AssertNoURLRequests();
}

IOThread::Globals::Globals() : system_request_context_leak_checker(this),
                               enable_brotli(false) {}

IOThread::Globals::~Globals() {}

// |local_state| is passed in explicitly in order to (1) reduce implicit
// dependencies and (2) make IOThread more flexible for testing.
IOThread::IOThread(
    PrefService* local_state,
    policy::PolicyService* policy_service,
    net_log::ChromeNetLog* net_log,
    extensions::EventRouterForwarder* extension_event_router_forwarder)
    : net_log_(net_log),
#if BUILDFLAG(ENABLE_EXTENSIONS)
      extension_event_router_forwarder_(extension_event_router_forwarder),
#endif
      globals_(nullptr),
      is_quic_allowed_by_policy_(true),
      http_09_on_non_default_ports_enabled_(false),
      creation_time_(base::TimeTicks::Now()),
      weak_factory_(this) {
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_proxy =
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO);
  auth_schemes_ = local_state->GetString(prefs::kAuthSchemes);
  negotiate_disable_cname_lookup_.Init(
      prefs::kDisableAuthNegotiateCnameLookup, local_state,
      base::Bind(&IOThread::UpdateNegotiateDisableCnameLookup,
                 base::Unretained(this)));
  negotiate_disable_cname_lookup_.MoveToThread(io_thread_proxy);
  negotiate_enable_port_.Init(
      prefs::kEnableAuthNegotiatePort, local_state,
      base::Bind(&IOThread::UpdateNegotiateEnablePort, base::Unretained(this)));
  negotiate_enable_port_.MoveToThread(io_thread_proxy);
  auth_server_whitelist_.Init(
      prefs::kAuthServerWhitelist, local_state,
      base::Bind(&IOThread::UpdateServerWhitelist, base::Unretained(this)));
  auth_server_whitelist_.MoveToThread(io_thread_proxy);
  auth_delegate_whitelist_.Init(
      prefs::kAuthNegotiateDelegateWhitelist, local_state,
      base::Bind(&IOThread::UpdateDelegateWhitelist, base::Unretained(this)));
  auth_delegate_whitelist_.MoveToThread(io_thread_proxy);
#if defined(OS_ANDROID)
  auth_android_negotiate_account_type_.Init(
      prefs::kAuthAndroidNegotiateAccountType, local_state,
      base::Bind(&IOThread::UpdateAndroidAuthNegotiateAccountType,
                 base::Unretained(this)));
  auth_android_negotiate_account_type_.MoveToThread(io_thread_proxy);
#endif
#if defined(OS_POSIX) && !defined(OS_ANDROID)
  gssapi_library_name_ = local_state->GetString(prefs::kGSSAPILibraryName);
#endif
  pref_proxy_config_tracker_.reset(
      ProxyServiceFactory::CreatePrefProxyConfigTrackerOfLocalState(
          local_state));
  ChromeNetworkDelegate::InitializePrefsOnUIThread(
      &system_enable_referrers_,
      nullptr,
      nullptr,
      nullptr,
      nullptr,
      local_state);
  ssl_config_service_manager_.reset(
      ssl_config::SSLConfigServiceManager::CreateDefaultManager(
          local_state,
          BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)));

  base::Value* dns_client_enabled_default = new base::FundamentalValue(
      chrome_browser_net::ConfigureAsyncDnsFieldTrial());
  local_state->SetDefaultPrefValue(prefs::kBuiltInDnsClientEnabled,
                                   dns_client_enabled_default);
  chrome_browser_net::LogAsyncDnsPrefSource(
      local_state->FindPreference(prefs::kBuiltInDnsClientEnabled));

  dns_client_enabled_.Init(prefs::kBuiltInDnsClientEnabled,
                           local_state,
                           base::Bind(&IOThread::UpdateDnsClientEnabled,
                                      base::Unretained(this)));
  dns_client_enabled_.MoveToThread(io_thread_proxy);

  quick_check_enabled_.Init(prefs::kQuickCheckEnabled,
                            local_state);
  quick_check_enabled_.MoveToThread(io_thread_proxy);

  pac_https_url_stripping_enabled_.Init(prefs::kPacHttpsUrlStrippingEnabled,
                                        local_state);
  pac_https_url_stripping_enabled_.MoveToThread(io_thread_proxy);

  const base::Value* value = policy_service->GetPolicies(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
      std::string())).GetValue(policy::key::kQuicAllowed);
  if (value)
    value->GetAsBoolean(&is_quic_allowed_by_policy_);

  value = policy_service
              ->GetPolicies(policy::PolicyNamespace(
                  policy::POLICY_DOMAIN_CHROME, std::string()))
              .GetValue(policy::key::kHttp09OnNonDefaultPortsEnabled);
  if (value)
    value->GetAsBoolean(&http_09_on_non_default_ports_enabled_);

  chrome_browser_net::SetGlobalSTHDistributor(
      std::unique_ptr<net::ct::STHDistributor>(new net::ct::STHDistributor()));

  BrowserThread::SetDelegate(BrowserThread::IO, this);
}

IOThread::~IOThread() {
  // This isn't needed for production code, but in tests, IOThread may
  // be multiply constructed.
  BrowserThread::SetDelegate(BrowserThread::IO, NULL);

  pref_proxy_config_tracker_->DetachFromPrefService();
  DCHECK(!globals_);

  // Destroy the old distributor to check that the observers list it holds is
  // empty.
  chrome_browser_net::SetGlobalSTHDistributor(nullptr);
}

IOThread::Globals* IOThread::globals() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return globals_;
}

void IOThread::SetGlobalsForTesting(Globals* globals) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!globals || !globals_);
  globals_ = globals;
}

net_log::ChromeNetLog* IOThread::net_log() {
  return net_log_;
}

void IOThread::ChangedToOnTheRecord() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&IOThread::ChangedToOnTheRecordOnIOThread,
                 base::Unretained(this)));
}

net::URLRequestContextGetter* IOThread::system_url_request_context_getter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!system_url_request_context_getter_.get()) {
    InitSystemRequestContext();
  }
  return system_url_request_context_getter_.get();
}

void IOThread::Init() {
  TRACE_EVENT0("startup", "IOThread::InitAsync");
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

#if defined(USE_NSS_CERTS)
  net::SetMessageLoopForNSSHttpIO();
#endif

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // Export ssl keys if log file specified.
  base::FilePath ssl_keylog_file = GetSSLKeyLogFile(command_line);
  if (!ssl_keylog_file.empty()) {
    net::SSLClientSocket::SetSSLKeyLogFile(
        ssl_keylog_file,
        BrowserThread::GetTaskRunnerForThread(BrowserThread::FILE));
  }

  DCHECK(!globals_);
  globals_ = new Globals;

  // Add an observer that will emit network change events to the ChromeNetLog.
  // Assuming NetworkChangeNotifier dispatches in FIFO order, we should be
  // logging the network change before other IO thread consumers respond to it.
  network_change_observer_.reset(
      new net::LoggingNetworkChangeObserver(net_log_));

  // Setup the HistogramWatcher to run on the IO thread.
  net::NetworkChangeNotifier::InitHistogramWatcher();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  globals_->extension_event_router_forwarder =
      extension_event_router_forwarder_;
#endif

  std::unique_ptr<data_usage::DataUseAmortizer> data_use_amortizer;
#if BUILDFLAG(ANDROID_JAVA_UI)
  data_use_amortizer.reset(new data_usage::android::TrafficStatsAmortizer());
#endif

  globals_->data_use_ascriber =
      base::MakeUnique<data_use_measurement::ChromeDataUseAscriber>();

  globals_->data_use_aggregator.reset(new data_usage::DataUseAggregator(
      std::unique_ptr<data_usage::DataUseAnnotator>(
          new chrome_browser_data_usage::TabIdAnnotator()),
      std::move(data_use_amortizer)));

  std::unique_ptr<ChromeNetworkDelegate> chrome_network_delegate(
      new ChromeNetworkDelegate(extension_event_router_forwarder(),
                                &system_enable_referrers_));
  // By default, data usage is considered off the record.
  chrome_network_delegate->set_data_use_aggregator(
      globals_->data_use_aggregator.get(),
      true /* is_data_usage_off_the_record */);

#if BUILDFLAG(ANDROID_JAVA_UI)
  globals_->external_data_use_observer.reset(
      new chrome::android::ExternalDataUseObserver(
          globals_->data_use_aggregator.get(),
          BrowserThread::GetTaskRunnerForThread(BrowserThread::IO),
          BrowserThread::GetTaskRunnerForThread(BrowserThread::UI)));
#endif

  globals_->system_network_delegate =
      globals_->data_use_ascriber->CreateNetworkDelegate(
          std::move(chrome_network_delegate), GetMetricsDataUseForwarder());

  globals_->host_resolver = CreateGlobalHostResolver(net_log_);

  std::map<std::string, std::string> network_quality_estimator_params;
  variations::GetVariationParams(kNetworkQualityEstimatorFieldTrialName,
                                 &network_quality_estimator_params);

  std::unique_ptr<net::ExternalEstimateProvider> external_estimate_provider;
#if BUILDFLAG(ANDROID_JAVA_UI)
  external_estimate_provider.reset(
      new chrome::android::ExternalEstimateProviderAndroid());
#endif
  // Pass ownership.
  globals_->network_quality_estimator.reset(new net::NetworkQualityEstimator(
      std::move(external_estimate_provider), network_quality_estimator_params));

  UpdateDnsClientEnabled();
#if defined(OS_CHROMEOS)
  // Creates a CertVerifyProc that doesn't allow any profile-provided certs.
  globals_->cert_verifier = base::MakeUnique<net::CachingCertVerifier>(
      base::MakeUnique<net::MultiThreadedCertVerifier>(
          new chromeos::CertVerifyProcChromeOS()));
#else
  globals_->cert_verifier = net::CertVerifier::CreateDefault();
#endif

  globals_->transport_security_state.reset(new net::TransportSecurityState());

  std::vector<scoped_refptr<const net::CTLogVerifier>> ct_logs(
      net::ct::CreateLogVerifiersForKnownLogs());

  globals_->ct_logs.assign(ct_logs.begin(), ct_logs.end());

  net::MultiLogCTVerifier* ct_verifier = new net::MultiLogCTVerifier();
  globals_->cert_transparency_verifier.reset(ct_verifier);
  // Add built-in logs
  ct_verifier->AddLogs(globals_->ct_logs);

  ct_tree_tracker_.reset(
      new certificate_transparency::TreeStateTracker(globals_->ct_logs));
  // Register the ct_tree_tracker_ as observer for new STHs.
  RegisterSTHObserver(ct_tree_tracker_.get());
  // Register the ct_tree_tracker_ as observer for verified SCTs.
  globals_->cert_transparency_verifier->SetObserver(ct_tree_tracker_.get());

  globals_->ct_policy_enforcer.reset(new net::CTPolicyEnforcer());
  params_.ct_policy_enforcer = globals_->ct_policy_enforcer.get();

  globals_->ssl_config_service = GetSSLConfigService();

  CreateDefaultAuthHandlerFactory();
  globals_->http_server_properties.reset(new net::HttpServerPropertiesImpl());
  // For the ProxyScriptFetcher, we use a direct ProxyService.
  globals_->proxy_script_fetcher_proxy_service =
      net::ProxyService::CreateDirectWithNetLog(net_log_);
  // In-memory cookie store.
  globals_->system_cookie_store =
      content::CreateCookieStore(content::CookieStoreConfig());
  // In-memory channel ID store.
  globals_->system_channel_id_service.reset(
      new net::ChannelIDService(
          new net::DefaultChannelIDStore(NULL),
          base::WorkerPool::GetTaskRunner(true)));
  globals_->system_cookie_store->SetChannelIDServiceID(
      globals_->system_channel_id_service->GetUniqueID());
  globals_->dns_probe_service.reset(new chrome_browser_net::DnsProbeService());
  globals_->host_mapping_rules.reset(new net::HostMappingRules());
  params_.host_mapping_rules = globals_->host_mapping_rules.get();
  globals_->http_user_agent_settings.reset(
      new net::StaticHttpUserAgentSettings(std::string(), GetUserAgent()));
  if (command_line.HasSwitch(switches::kHostRules)) {
    TRACE_EVENT_BEGIN0("startup", "IOThread::InitAsync:SetRulesFromString");
    globals_->host_mapping_rules->SetRulesFromString(
        command_line.GetSwitchValueASCII(switches::kHostRules));
    TRACE_EVENT_END0("startup", "IOThread::InitAsync:SetRulesFromString");
  }
  globals_->enable_brotli =
      base::FeatureList::IsEnabled(features::kBrotliEncoding);
  params_.enable_token_binding =
      base::FeatureList::IsEnabled(features::kTokenBinding);

  // Check for OS support of TCP FastOpen, and turn it on for all connections if
  // indicated by user.
  // TODO(rch): Make the client socket factory a per-network session instance,
  // constructed from a NetworkSession::Params, to allow us to move this option
  // to IOThread::Globals & HttpNetworkSession::Params.
  bool always_enable_tfo_if_supported =
      command_line.HasSwitch(switches::kEnableTcpFastOpen);
  net::CheckSupportAndMaybeEnableTCPFastOpen(always_enable_tfo_if_supported);

  ConfigureParamsFromFieldTrialsAndCommandLine(
      command_line, is_quic_allowed_by_policy_,
      http_09_on_non_default_ports_enabled_, &params_);

  TRACE_EVENT_BEGIN0("startup",
                     "IOThread::Init:ProxyScriptFetcherRequestContext");
  globals_->proxy_script_fetcher_context.reset(
      ConstructProxyScriptFetcherContext(globals_, params_, net_log_));
  TRACE_EVENT_END0("startup",
                   "IOThread::Init:ProxyScriptFetcherRequestContext");

#if defined(OS_MACOSX)
  // Start observing Keychain events. This needs to be done on the UI thread,
  // as Keychain services requires a CFRunLoop.
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&ObserveKeychainEvents));
#endif

  // InitSystemRequestContext turns right around and posts a task back
  // to the IO thread, so we can't let it run until we know the IO
  // thread has started.
  //
  // Note that since we are at BrowserThread::Init time, the UI thread
  // is blocked waiting for the thread to start.  Therefore, posting
  // this task to the main thread's message loop here is guaranteed to
  // get it onto the message loop while the IOThread object still
  // exists.  However, the message might not be processed on the UI
  // thread until after IOThread is gone, so use a weak pointer.
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&IOThread::InitSystemRequestContext,
                                     weak_factory_.GetWeakPtr()));

#if defined(OS_ANDROID) && defined(ARCH_CPU_ARMEL)
  // Record how common CPUs with broken NEON units are. See
  // https://crbug.com/341598.
  crypto::EnsureOpenSSLInit();
  UMA_HISTOGRAM_BOOLEAN("Net.HasBrokenNEON", CRYPTO_has_broken_NEON());
#endif
}

void IOThread::CleanUp() {
  base::debug::LeakTracker<SafeBrowsingURLRequestContext>::CheckForLeaks();

#if defined(USE_NSS_CERTS)
  net::ShutdownNSSHttpIO();
#endif

  system_url_request_context_getter_ = NULL;

  // Unlink the ct_tree_tracker_ from the global cert_transparency_verifier
  // and unregister it from new STH notifications so it will take no actions
  // on anything observed during CleanUp process.
  globals()->cert_transparency_verifier->SetObserver(nullptr);
  UnregisterSTHObserver(ct_tree_tracker_.get());

  ct_tree_tracker_.reset();

  // Release objects that the net::URLRequestContext could have been pointing
  // to.

  // Shutdown the HistogramWatcher on the IO thread.
  net::NetworkChangeNotifier::ShutdownHistogramWatcher();

  // This must be reset before the ChromeNetLog is destroyed.
  network_change_observer_.reset();

  system_proxy_config_service_.reset();
  delete globals_;
  globals_ = NULL;

  base::debug::LeakTracker<SystemURLRequestContextGetter>::CheckForLeaks();
}

// static
void IOThread::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kAuthSchemes,
                               "basic,digest,ntlm,negotiate");
  registry->RegisterBooleanPref(prefs::kDisableAuthNegotiateCnameLookup, false);
  registry->RegisterBooleanPref(prefs::kEnableAuthNegotiatePort, false);
  registry->RegisterStringPref(prefs::kAuthServerWhitelist, std::string());
  registry->RegisterStringPref(prefs::kAuthNegotiateDelegateWhitelist,
                               std::string());
  registry->RegisterStringPref(prefs::kGSSAPILibraryName, std::string());
  registry->RegisterStringPref(prefs::kAuthAndroidNegotiateAccountType,
                               std::string());
  registry->RegisterBooleanPref(prefs::kEnableReferrers, true);
  data_reduction_proxy::RegisterPrefs(registry);
  registry->RegisterBooleanPref(prefs::kBuiltInDnsClientEnabled, true);
  registry->RegisterBooleanPref(prefs::kQuickCheckEnabled, true);
  registry->RegisterBooleanPref(prefs::kPacHttpsUrlStrippingEnabled, true);
}

void IOThread::UpdateServerWhitelist() {
  globals_->http_auth_preferences->set_server_whitelist(
      auth_server_whitelist_.GetValue());
}

void IOThread::UpdateDelegateWhitelist() {
  globals_->http_auth_preferences->set_delegate_whitelist(
      auth_delegate_whitelist_.GetValue());
}

#if defined(OS_ANDROID)
void IOThread::UpdateAndroidAuthNegotiateAccountType() {
  globals_->http_auth_preferences->set_auth_android_negotiate_account_type(
      auth_android_negotiate_account_type_.GetValue());
}
#endif

void IOThread::UpdateNegotiateDisableCnameLookup() {
  globals_->http_auth_preferences->set_negotiate_disable_cname_lookup(
      negotiate_disable_cname_lookup_.GetValue());
}

void IOThread::UpdateNegotiateEnablePort() {
  globals_->http_auth_preferences->set_negotiate_enable_port(
      negotiate_enable_port_.GetValue());
}

void IOThread::CreateDefaultAuthHandlerFactory() {
  std::vector<std::string> supported_schemes = base::SplitString(
      auth_schemes_, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  globals_->http_auth_preferences.reset(new net::HttpAuthPreferences(
      supported_schemes
#if defined(OS_POSIX) && !defined(OS_ANDROID)
      ,
      gssapi_library_name_
#endif
      ));
  UpdateServerWhitelist();
  UpdateDelegateWhitelist();
  UpdateNegotiateDisableCnameLookup();
  UpdateNegotiateEnablePort();
#if defined(OS_ANDROID)
  UpdateAndroidAuthNegotiateAccountType();
#endif
  globals_->http_auth_handler_factory =
      net::HttpAuthHandlerRegistryFactory::Create(
          globals_->http_auth_preferences.get(), globals_->host_resolver.get());
}

void IOThread::ClearHostCache(
    const base::Callback<bool(const std::string&)>& host_filter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  net::HostCache* host_cache = globals_->host_resolver->GetHostCache();
  if (host_cache)
    host_cache->ClearForHosts(host_filter);
}

const net::HttpNetworkSession::Params& IOThread::NetworkSessionParams() const {
  return params_;
}

base::TimeTicks IOThread::creation_time() const {
  return creation_time_;
}

net::SSLConfigService* IOThread::GetSSLConfigService() {
  return ssl_config_service_manager_->Get();
}

void IOThread::ChangedToOnTheRecordOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Clear the host cache to avoid showing entries from the OTR session
  // in about:net-internals.
  ClearHostCache(base::Callback<bool(const std::string&)>());
}

void IOThread::InitSystemRequestContext() {
  if (system_url_request_context_getter_.get())
    return;
  // If we're in unit_tests, IOThread may not be run.
  if (!BrowserThread::IsMessageLoopValid(BrowserThread::IO))
    return;
  system_proxy_config_service_ = ProxyServiceFactory::CreateProxyConfigService(
      pref_proxy_config_tracker_.get());
  system_url_request_context_getter_ =
      new SystemURLRequestContextGetter(this);
  // Safe to post an unretained this pointer, since IOThread is
  // guaranteed to outlive the IO BrowserThread.
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&IOThread::InitSystemRequestContextOnIOThread,
                 base::Unretained(this)));
}

void IOThread::InitSystemRequestContextOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!globals_->system_proxy_service.get());
  DCHECK(system_proxy_config_service_.get());

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  globals_->system_proxy_service = ProxyServiceFactory::CreateProxyService(
      net_log_, globals_->proxy_script_fetcher_context.get(),
      globals_->system_network_delegate.get(),
      std::move(system_proxy_config_service_), command_line,
      WpadQuickCheckEnabled(), PacHttpsUrlStrippingEnabled());

  globals_->system_request_context.reset(
      ConstructSystemRequestContext(globals_, params_, net_log_));
}

void IOThread::UpdateDnsClientEnabled() {
  globals()->host_resolver->SetDnsClientEnabled(*dns_client_enabled_);
}

void IOThread::RegisterSTHObserver(net::ct::STHObserver* observer) {
  chrome_browser_net::GetGlobalSTHDistributor()->RegisterObserver(observer);
}

void IOThread::UnregisterSTHObserver(net::ct::STHObserver* observer) {
  chrome_browser_net::GetGlobalSTHDistributor()->UnregisterObserver(observer);
}

bool IOThread::WpadQuickCheckEnabled() const {
  return quick_check_enabled_.GetValue();
}

bool IOThread::PacHttpsUrlStrippingEnabled() const {
  return pac_https_url_stripping_enabled_.GetValue();
}

// static
net::URLRequestContext* IOThread::ConstructSystemRequestContext(
    IOThread::Globals* globals,
    const net::HttpNetworkSession::Params& params,
    net::NetLog* net_log) {
  net::URLRequestContext* context = new SystemURLRequestContext;
  context->set_net_log(net_log);
  context->set_host_resolver(globals->host_resolver.get());
  context->set_cert_verifier(globals->cert_verifier.get());
  context->set_transport_security_state(
      globals->transport_security_state.get());
  context->set_cert_transparency_verifier(
      globals->cert_transparency_verifier.get());
  context->set_ct_policy_enforcer(globals->ct_policy_enforcer.get());
  context->set_ssl_config_service(globals->ssl_config_service.get());
  context->set_http_auth_handler_factory(
      globals->http_auth_handler_factory.get());
  context->set_proxy_service(globals->system_proxy_service.get());

  globals->system_url_request_job_factory.reset(
      new net::URLRequestJobFactoryImpl());
  context->set_job_factory(globals->system_url_request_job_factory.get());

  context->set_cookie_store(globals->system_cookie_store.get());
  context->set_channel_id_service(
      globals->system_channel_id_service.get());
  context->set_network_delegate(globals->system_network_delegate.get());
  context->set_http_user_agent_settings(
      globals->http_user_agent_settings.get());
  context->set_network_quality_estimator(
      globals->network_quality_estimator.get());

  context->set_http_server_properties(globals->http_server_properties.get());

  context->set_enable_brotli(globals->enable_brotli);

  net::HttpNetworkSession::Params system_params(params);
  net::URLRequestContextBuilder::SetHttpNetworkSessionComponents(
      context, &system_params);

  globals->system_http_network_session.reset(
      new net::HttpNetworkSession(system_params));
  globals->system_http_transaction_factory.reset(
      new net::HttpNetworkLayer(globals->system_http_network_session.get()));
  context->set_http_transaction_factory(
      globals->system_http_transaction_factory.get());

  return context;
}

// static
void IOThread::ConfigureParamsFromFieldTrialsAndCommandLine(
    const base::CommandLine& command_line,
    bool is_quic_allowed_by_policy,
    bool http_09_on_non_default_ports_enabled,
    net::HttpNetworkSession::Params* params) {
  std::string quic_user_agent_id = chrome::GetChannelString();
  if (!quic_user_agent_id.empty())
    quic_user_agent_id.push_back(' ');
  quic_user_agent_id.append(
      version_info::GetProductNameAndVersionForUserAgent());
  quic_user_agent_id.push_back(' ');
  quic_user_agent_id.append(content::BuildOSCpuInfo());

  bool is_quic_force_disabled = !is_quic_allowed_by_policy ||
                                command_line.HasSwitch(switches::kDisableQuic);
  bool is_quic_force_enabled = command_line.HasSwitch(switches::kEnableQuic);

  network_session_configurator::ParseFieldTrials(is_quic_force_disabled,
                                                 is_quic_force_enabled,
                                                 quic_user_agent_id, params);

  // Command line flags override field trials.
  if (command_line.HasSwitch(switches::kIgnoreUrlFetcherCertRequests))
    net::URLFetcher::SetIgnoreCertificateRequests(true);

  if (command_line.HasSwitch(switches::kDisableHttp2))
    params->enable_http2 = false;

  if (params->enable_quic) {
    if (command_line.HasSwitch(switches::kQuicConnectionOptions)) {
      params->quic_connection_options =
          net::QuicUtils::ParseQuicConnectionOptions(
              command_line.GetSwitchValueASCII(
                  switches::kQuicConnectionOptions));
    }

    if (command_line.HasSwitch(switches::kQuicHostWhitelist)) {
      std::string whitelist =
          command_line.GetSwitchValueASCII(switches::kQuicHostWhitelist);
      params->quic_host_whitelist.clear();
      for (const std::string& host : base::SplitString(
               whitelist, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
        params->quic_host_whitelist.insert(host);
      }
    }

    if (command_line.HasSwitch(switches::kQuicMaxPacketLength)) {
      unsigned value;
      if (base::StringToUint(
              command_line.GetSwitchValueASCII(switches::kQuicMaxPacketLength),
              &value)) {
        params->quic_max_packet_length = value;
      }
    }

    if (command_line.HasSwitch(switches::kQuicVersion)) {
      net::QuicVersion version = network_session_configurator::ParseQuicVersion(
          command_line.GetSwitchValueASCII(switches::kQuicVersion));
      if (version != net::QUIC_VERSION_UNSUPPORTED) {
        net::QuicVersionVector supported_versions;
        supported_versions.push_back(version);
        params->quic_supported_versions = supported_versions;
      }
    }

    if (command_line.HasSwitch(switches::kOriginToForceQuicOn)) {
      std::string origins =
          command_line.GetSwitchValueASCII(switches::kOriginToForceQuicOn);
      for (const std::string& host_port : base::SplitString(
               origins, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
        if (host_port == "*")
          params->origins_to_force_quic_on.insert(net::HostPortPair());
        net::HostPortPair quic_origin =
            net::HostPortPair::FromString(host_port);
        if (!quic_origin.IsEmpty())
          params->origins_to_force_quic_on.insert(quic_origin);
      }
    }
  }

  // Parameters only controlled by command line.
  if (command_line.HasSwitch(switches::kEnableUserAlternateProtocolPorts)) {
    params->enable_user_alternate_protocol_ports = true;
  }
  if (command_line.HasSwitch(switches::kIgnoreCertificateErrors))
    params->ignore_certificate_errors = true;
  if (command_line.HasSwitch(switches::kTestingFixedHttpPort)) {
    params->testing_fixed_http_port =
        GetSwitchValueAsInt(command_line, switches::kTestingFixedHttpPort);
  }
  if (command_line.HasSwitch(switches::kTestingFixedHttpsPort)) {
    params->testing_fixed_https_port =
        GetSwitchValueAsInt(command_line, switches::kTestingFixedHttpsPort);
  }

  params->http_09_on_non_default_ports_enabled =
      http_09_on_non_default_ports_enabled;
}

// static
net::URLRequestContext* IOThread::ConstructProxyScriptFetcherContext(
    IOThread::Globals* globals,
    const net::HttpNetworkSession::Params& params,
    net::NetLog* net_log) {
  net::URLRequestContext* context = new net::URLRequestContext;
  context->set_net_log(net_log);
  context->set_host_resolver(globals->host_resolver.get());
  context->set_cert_verifier(globals->cert_verifier.get());
  context->set_transport_security_state(
      globals->transport_security_state.get());
  context->set_cert_transparency_verifier(
      globals->cert_transparency_verifier.get());
  context->set_ct_policy_enforcer(globals->ct_policy_enforcer.get());
  context->set_ssl_config_service(globals->ssl_config_service.get());
  context->set_http_auth_handler_factory(
      globals->http_auth_handler_factory.get());
  context->set_proxy_service(globals->proxy_script_fetcher_proxy_service.get());

  context->set_job_factory(
      globals->proxy_script_fetcher_url_request_job_factory.get());

  context->set_cookie_store(globals->system_cookie_store.get());
  context->set_channel_id_service(
      globals->system_channel_id_service.get());
  context->set_network_delegate(globals->system_network_delegate.get());
  context->set_http_user_agent_settings(
      globals->http_user_agent_settings.get());
  context->set_http_server_properties(globals->http_server_properties.get());

  context->set_enable_brotli(globals->enable_brotli);

  net::HttpNetworkSession::Params session_params(params);
  net::URLRequestContextBuilder::SetHttpNetworkSessionComponents(
      context, &session_params);

  globals->proxy_script_fetcher_http_network_session.reset(
      new net::HttpNetworkSession(session_params));
  globals->proxy_script_fetcher_http_transaction_factory.reset(
      new net::HttpNetworkLayer(
          globals->proxy_script_fetcher_http_network_session.get()));
  context->set_http_transaction_factory(
      globals->proxy_script_fetcher_http_transaction_factory.get());

  std::unique_ptr<net::URLRequestJobFactoryImpl> job_factory(
      new net::URLRequestJobFactoryImpl());

  job_factory->SetProtocolHandler(url::kDataScheme,
                                  base::MakeUnique<net::DataProtocolHandler>());
  job_factory->SetProtocolHandler(
      url::kFileScheme,
      base::MakeUnique<net::FileProtocolHandler>(
          content::BrowserThread::GetBlockingPool()
              ->GetTaskRunnerWithShutdownBehavior(
                  base::SequencedWorkerPool::SKIP_ON_SHUTDOWN)));
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
  job_factory->SetProtocolHandler(
      url::kFtpScheme,
      net::FtpProtocolHandler::Create(globals->host_resolver.get()));
#endif
  globals->proxy_script_fetcher_url_request_job_factory =
      std::move(job_factory);

  context->set_job_factory(
      globals->proxy_script_fetcher_url_request_job_factory.get());

  // TODO(rtenneti): We should probably use HttpServerPropertiesManager for the
  // system URLRequestContext too. There's no reason this should be tied to a
  // profile.
  return context;
}

metrics::UpdateUsagePrefCallbackType IOThread::GetMetricsDataUseForwarder() {
  return base::Bind(&UpdateMetricsUsagePrefsOnUIThread);
}

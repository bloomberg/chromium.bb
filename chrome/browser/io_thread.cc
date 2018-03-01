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
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/data_usage/tab_id_annotator.h"
#include "chrome/browser/data_use_measurement/chrome_data_use_ascriber.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/dns_probe_service.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/browser/net/sth_distributor_provider.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
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
#include "components/network_session_configurator/common/network_features.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_quality_observer_factory.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "extensions/features/features.h"
#include "net/cert/caching_cert_verifier.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/ct_known_logs.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cert/multi_threaded_cert_verifier.h"
#include "net/cert/sth_distributor.h"
#include "net/cert/sth_observer.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/http/http_auth_filter.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_auth_preferences.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/http_transaction_factory.h"
#include "net/net_features.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/proxy_resolution/pac_file_fetcher_impl.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/proxy_resolution/proxy_service.h"
#include "net/quic/chromium/quic_utils_chromium.h"
#include "net/socket/ssl_client_socket.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/ignore_errors_cert_verifier.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/url_request_context_builder_mojo.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/event_router_forwarder.h"
#endif

#if defined(USE_NSS_CERTS)
#include "net/cert_net/nss_ocsp.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#include "chrome/browser/android/data_usage/external_data_use_observer.h"
#include "components/data_usage/android/traffic_stats_amortizer.h"
#include "net/cert/cert_verify_proc_android.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/net/cert_verify_proc_chromeos.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chromeos/network/dhcp_pac_file_fetcher_factory_chromeos.h"
#include "chromeos/network/host_resolver_impl_chromeos.h"
#endif

#if defined(OS_ANDROID) && defined(ARCH_CPU_ARMEL)
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/cpu.h"
#endif

#if defined(OS_ANDROID) || defined(OS_FUCHSIA)
#include "net/cert/cert_net_fetcher.h"
#include "net/cert_net/cert_net_fetcher_impl.h"
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

std::unique_ptr<net::HostResolver> CreateGlobalHostResolver(
    net::NetLog* net_log) {
  TRACE_EVENT0("startup", "IOThread::CreateGlobalHostResolver");

#if defined(OS_CHROMEOS)
  using resolver = chromeos::HostResolverImplChromeOS;
#else
  using resolver = net::HostResolver;
#endif
  std::unique_ptr<net::HostResolver> global_host_resolver =
      resolver::CreateSystemResolver(net::HostResolver::Options(), net_log);

  // If hostname remappings were specified on the command-line, layer these
  // rules on top of the real host resolver. This allows forwarding all requests
  // through a designated test server.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(network::switches::kHostResolverRules))
    return global_host_resolver;

  auto remapped_resolver = std::make_unique<net::MappedHostResolver>(
      std::move(global_host_resolver));
  remapped_resolver->SetRulesFromString(
      command_line.GetSwitchValueASCII(network::switches::kHostResolverRules));
  return std::move(remapped_resolver);
}

// This function is for forwarding metrics usage pref changes to the metrics
// service on the appropriate thread.
// TODO(gayane): Reduce the frequency of posting tasks from IO to UI thread.
void UpdateMetricsUsagePrefsOnUIThread(const std::string& service_name,
                                       int message_size,
                                       bool is_cellular) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::BindOnce(
                              [](const std::string& service_name,
                                 int message_size, bool is_cellular) {
                                // Some unit tests use IOThread but do not
                                // initialize MetricsService. In that case it's
                                // fine to skip the update.
                                auto* metrics_service =
                                    g_browser_process->metrics_service();
                                if (metrics_service) {
                                  metrics_service->UpdateMetricsUsagePrefs(
                                      service_name, message_size, is_cellular);
                                }
                              },
                              service_name, message_size, is_cellular));
}

// Check the AsyncDns field trial and return true if it should be enabled. On
// Android this includes checking the Android version in the field trial.
bool ShouldEnableAsyncDns() {
  bool feature_can_be_enabled = true;
#if defined(OS_ANDROID)
  int min_sdk =
      base::GetFieldTrialParamByFeatureAsInt(features::kAsyncDns, "min_sdk", 0);
  if (base::android::BuildInfo::GetInstance()->sdk_int() < min_sdk)
    feature_can_be_enabled = false;
#endif
  return feature_can_be_enabled &&
         base::FeatureList::IsEnabled(features::kAsyncDns);
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
  DCHECK(io_thread_->globals()->system_request_context);

  return io_thread_->globals()->system_request_context;
}

scoped_refptr<base::SingleThreadTaskRunner>
SystemURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

IOThread::Globals::Globals() : system_request_context(nullptr) {}

IOThread::Globals::~Globals() {}

// |local_state| is passed in explicitly in order to (1) reduce implicit
// dependencies and (2) make IOThread more flexible for testing.
IOThread::IOThread(
    PrefService* local_state,
    policy::PolicyService* policy_service,
    net_log::ChromeNetLog* net_log,
    extensions::EventRouterForwarder* extension_event_router_forwarder,
    SystemNetworkContextManager* system_network_context_manager)
    : net_log_(net_log),
#if BUILDFLAG(ENABLE_EXTENSIONS)
      extension_event_router_forwarder_(extension_event_router_forwarder),
#endif
      globals_(nullptr),
      is_quic_allowed_on_init_(true),
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
#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  gssapi_library_name_ = local_state->GetString(prefs::kGSSAPILibraryName);
#endif
#if defined(OS_CHROMEOS)
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  allow_gssapi_library_load_ = connector->IsActiveDirectoryManaged();
#endif
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

  local_state->SetDefaultPrefValue(prefs::kBuiltInDnsClientEnabled,
                                   base::Value(ShouldEnableAsyncDns()));

  dns_client_enabled_.Init(prefs::kBuiltInDnsClientEnabled,
                           local_state,
                           base::Bind(&IOThread::UpdateDnsClientEnabled,
                                      base::Unretained(this)));
  dns_client_enabled_.MoveToThread(io_thread_proxy);

  if (base::FeatureList::IsEnabled(features::kDnsOverHttps)) {
    base::Value specs(base::Value::Type::LIST);
    base::Value methods(base::Value::Type::LIST);
    base::Value spec(variations::GetVariationParamValueByFeature(
        features::kDnsOverHttps, "server"));
    base::Value method(variations::GetVariationParamValueByFeature(
        features::kDnsOverHttps, "method"));
    if (spec.GetString().size() > 0) {
      specs.GetList().push_back(std::move(spec));
      methods.GetList().push_back(std::move(method));
      local_state->SetDefaultPrefValue(prefs::kDnsOverHttpsServers,
                                       std::move(specs));
      local_state->SetDefaultPrefValue(prefs::kDnsOverHttpsServerMethods,
                                       std::move(methods));
    }
  }
  dns_over_https_servers_.Init(
      prefs::kDnsOverHttpsServers, local_state,
      base::Bind(&IOThread::UpdateDnsClientEnabled, base::Unretained(this)));
  dns_over_https_server_methods_.Init(
      prefs::kDnsOverHttpsServerMethods, local_state,
      base::Bind(&IOThread::UpdateDnsClientEnabled, base::Unretained(this)));
  dns_over_https_servers_.MoveToThread(io_thread_proxy);
  dns_over_https_server_methods_.MoveToThread(io_thread_proxy);

#if defined(OS_POSIX)
  local_state->SetDefaultPrefValue(
      prefs::kNtlmV2Enabled,
      base::Value(base::FeatureList::IsEnabled(features::kNtlmV2Enabled)));
  ntlm_v2_enabled_.Init(
      prefs::kNtlmV2Enabled, local_state,
      base::Bind(&IOThread::UpdateNtlmV2Enabled, base::Unretained(this)));
  ntlm_v2_enabled_.MoveToThread(io_thread_proxy);
#endif

  quick_check_enabled_.Init(prefs::kQuickCheckEnabled,
                            local_state);
  quick_check_enabled_.MoveToThread(io_thread_proxy);

  pac_https_url_stripping_enabled_.Init(prefs::kPacHttpsUrlStrippingEnabled,
                                        local_state);
  pac_https_url_stripping_enabled_.MoveToThread(io_thread_proxy);

  chrome_browser_net::SetGlobalSTHDistributor(
      std::make_unique<net::ct::STHDistributor>());

  BrowserThread::SetIOThreadDelegate(this);

  system_network_context_manager->SetUp(&network_context_request_,
                                        &network_context_params_,
                                        &is_quic_allowed_on_init_);
}

IOThread::~IOThread() {
  // This isn't needed for production code, but in tests, IOThread may
  // be multiply constructed.
  BrowserThread::SetIOThreadDelegate(nullptr);

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
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&IOThread::ChangedToOnTheRecordOnIOThread,
                     base::Unretained(this)));
}

net::URLRequestContextGetter* IOThread::system_url_request_context_getter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!system_url_request_context_getter_.get()) {
    system_url_request_context_getter_ =
        base::MakeRefCounted<SystemURLRequestContextGetter>(this);
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
  if (!ssl_keylog_file.empty())
    net::SSLClientSocket::SetSSLKeyLogFile(ssl_keylog_file);

  DCHECK(!globals_);
  globals_ = new Globals;

  // Setup the HistogramWatcher to run on the IO thread.
  net::NetworkChangeNotifier::InitHistogramWatcher();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  globals_->extension_event_router_forwarder =
      extension_event_router_forwarder_;
#endif

  std::unique_ptr<data_usage::DataUseAmortizer> data_use_amortizer;
#if defined(OS_ANDROID)
  data_use_amortizer =
      std::make_unique<data_usage::android::TrafficStatsAmortizer>();
#endif  // defined(OS_ANDROID)

  globals_->data_use_ascriber =
      std::make_unique<data_use_measurement::ChromeDataUseAscriber>();

  globals_->data_use_aggregator =
      std::make_unique<data_usage::DataUseAggregator>(
          std::make_unique<chrome_browser_data_usage::TabIdAnnotator>(),
          std::move(data_use_amortizer));

#if defined(OS_ANDROID)
  globals_->external_data_use_observer =
      std::make_unique<android::ExternalDataUseObserver>(
          globals_->data_use_aggregator.get(),
          BrowserThread::GetTaskRunnerForThread(BrowserThread::IO),
          BrowserThread::GetTaskRunnerForThread(BrowserThread::UI));
#endif  // defined(OS_ANDROID)

  std::map<std::string, std::string> network_quality_estimator_params;
  variations::GetVariationParams(kNetworkQualityEstimatorFieldTrialName,
                                 &network_quality_estimator_params);

  if (command_line.HasSwitch(switches::kForceEffectiveConnectionType)) {
    const std::string force_ect_value =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kForceEffectiveConnectionType);

    if (!force_ect_value.empty()) {
      // If the effective connection type is forced using command line switch,
      // it overrides the one set by field trial.
      network_quality_estimator_params[net::kForceEffectiveConnectionType] =
          force_ect_value;
    }
  }

  // Pass ownership.
  globals_->network_quality_estimator =
      std::make_unique<net::NetworkQualityEstimator>(
          std::make_unique<net::NetworkQualityEstimatorParams>(
              network_quality_estimator_params),
          net_log_);
  globals_->network_quality_observer = content::CreateNetworkQualityObserver(
      globals_->network_quality_estimator.get());

  globals_->dns_probe_service =
      std::make_unique<chrome_browser_net::DnsProbeService>();

  if (command_line.HasSwitch(switches::kIgnoreUrlFetcherCertRequests))
    net::URLFetcher::SetIgnoreCertificateRequests(true);

#if defined(OS_MACOSX)
  // Start observing Keychain events. This needs to be done on the UI thread,
  // as Keychain services requires a CFRunLoop.
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&ObserveKeychainEvents));
#endif

#if defined(OS_ANDROID) && defined(ARCH_CPU_ARMEL)
  crypto::EnsureOpenSSLInit();
  // Measure CPUs with broken NEON units. See https://crbug.com/341598.
  UMA_HISTOGRAM_BOOLEAN("Net.HasBrokenNEON", CRYPTO_has_broken_NEON());
  // Measure Android kernels with missing AT_HWCAP2 auxv fields. See
  // https://crbug.com/boringssl/46.
  UMA_HISTOGRAM_BOOLEAN("Net.NeedsHWCAP2Workaround",
                        CRYPTO_needs_hwcap2_workaround());
#endif

  std::vector<scoped_refptr<const net::CTLogVerifier>> ct_logs(
      net::ct::CreateLogVerifiersForKnownLogs());
  globals_->ct_logs.assign(ct_logs.begin(), ct_logs.end());

  ConstructSystemRequestContext();

  UpdateDnsClientEnabled();

  ct_tree_tracker_ =
      std::make_unique<certificate_transparency::TreeStateTracker>(
          globals_->ct_logs, globals_->system_request_context->host_resolver(),
          net_log_);
  // Register the ct_tree_tracker_ as observer for new STHs.
  RegisterSTHObserver(ct_tree_tracker_.get());
  // Register the ct_tree_tracker_ as observer for verified SCTs.
  globals_->system_request_context->cert_transparency_verifier()->SetObserver(
      ct_tree_tracker_.get());
}

void IOThread::CleanUp() {
  base::debug::LeakTracker<SafeBrowsingURLRequestContext>::CheckForLeaks();

#if defined(USE_NSS_CERTS)
  net::ShutdownNSSHttpIO();
#endif

  system_url_request_context_getter_ = nullptr;

  // Unlink the ct_tree_tracker_ from the global cert_transparency_verifier
  // and unregister it from new STH notifications so it will take no actions
  // on anything observed during CleanUp process.
  globals()->system_request_context->cert_transparency_verifier()->SetObserver(
      nullptr);
  UnregisterSTHObserver(ct_tree_tracker_.get());
  ct_tree_tracker_.reset();

  globals_->system_request_context->proxy_resolution_service()->OnShutdown();

#if defined(USE_NSS_CERTS)
  net::SetURLRequestContextForNSSHttpIO(nullptr);
#endif

#if defined(OS_ANDROID) || defined(OS_FUCHSIA)
  net::ShutdownGlobalCertNetFetcher();
#endif

  // Release objects that the net::URLRequestContext could have been pointing
  // to.

  // Shutdown the HistogramWatcher on the IO thread.
  net::NetworkChangeNotifier::ShutdownHistogramWatcher();

  delete globals_;
  globals_ = nullptr;

  base::debug::LeakTracker<SystemURLRequestContextGetter>::CheckForLeaks();

  if (net_log_)
    net_log_->ShutDownBeforeTaskScheduler();
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
  registry->RegisterListPref(prefs::kDnsOverHttpsServers);
  registry->RegisterListPref(prefs::kDnsOverHttpsServerMethods);
  registry->RegisterBooleanPref(prefs::kQuickCheckEnabled, true);
  registry->RegisterBooleanPref(prefs::kPacHttpsUrlStrippingEnabled, true);
#if defined(OS_POSIX)
  registry->RegisterBooleanPref(prefs::kNtlmV2Enabled, true);
#endif
}

void IOThread::UpdateServerWhitelist() {
  globals_->http_auth_preferences->SetServerWhitelist(
      auth_server_whitelist_.GetValue());
}

void IOThread::UpdateDelegateWhitelist() {
  globals_->http_auth_preferences->SetDelegateWhitelist(
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

#if defined(OS_POSIX)
void IOThread::UpdateNtlmV2Enabled() {
  globals_->http_auth_preferences->set_ntlm_v2_enabled(
      ntlm_v2_enabled_.GetValue());
}
#endif

std::unique_ptr<net::HttpAuthHandlerFactory>
IOThread::CreateDefaultAuthHandlerFactory(net::HostResolver* host_resolver) {
  std::vector<std::string> supported_schemes = base::SplitString(
      auth_schemes_, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  globals_->http_auth_preferences =
      std::make_unique<net::HttpAuthPreferences>(supported_schemes
#if defined(OS_CHROMEOS)
                                                 ,
                                                 allow_gssapi_library_load_
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
                                                 ,
                                                 gssapi_library_name_
#endif
                                                 );
  UpdateServerWhitelist();
  UpdateDelegateWhitelist();
  UpdateNegotiateDisableCnameLookup();
  UpdateNegotiateEnablePort();
#if defined(OS_POSIX)
  UpdateNtlmV2Enabled();
#endif
#if defined(OS_ANDROID)
  UpdateAndroidAuthNegotiateAccountType();
#endif

  return net::HttpAuthHandlerRegistryFactory::Create(
      globals_->http_auth_preferences.get(), host_resolver);
}

void IOThread::ClearHostCache(
    const base::Callback<bool(const std::string&)>& host_filter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  globals_->system_request_context->host_resolver()
      ->GetHostCache()
      ->ClearForHosts(host_filter);
}

void IOThread::DisableQuic() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  globals_->quic_disabled = true;
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

void IOThread::UpdateDnsClientEnabled() {
  globals()->system_request_context->host_resolver()->SetDnsClientEnabled(
      *dns_client_enabled_ || (*dns_over_https_servers_).size() > 0);

  net::HostResolver* resolver =
      globals()->system_request_context->host_resolver();
  if ((*dns_over_https_servers_).size()) {
    resolver->SetRequestContext(globals_->system_request_context);
  }
  DCHECK((*dns_over_https_servers_).size() ==
         (*dns_over_https_server_methods_).size());
  resolver->ClearDnsOverHttpsServers();
  for (unsigned int i = 0; i < (*dns_over_https_servers_).size(); i++) {
    resolver->AddDnsOverHttpsServer(
        (*dns_over_https_servers_)[i],
        (*dns_over_https_server_methods_)[i].compare("POST") == 0);
  }
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

void IOThread::SetUpProxyService(
    network::URLRequestContextBuilderMojo* builder) const {
#if defined(OS_CHROMEOS)
  builder->SetDhcpFetcherFactory(
      std::make_unique<chromeos::DhcpProxyScriptFetcherFactoryChromeos>());
#endif

  builder->set_pac_quick_check_enabled(WpadQuickCheckEnabled());
  builder->set_pac_sanitize_url_policy(
      PacHttpsUrlStrippingEnabled()
          ? net::ProxyResolutionService::SanitizeUrlPolicy::SAFE
          : net::ProxyResolutionService::SanitizeUrlPolicy::UNSAFE);
}

certificate_transparency::TreeStateTracker* IOThread::ct_tree_tracker() const {
  return ct_tree_tracker_.get();
}

void IOThread::ConstructSystemRequestContext() {
  std::unique_ptr<network::URLRequestContextBuilderMojo> builder =
      std::make_unique<network::URLRequestContextBuilderMojo>();

  builder->set_network_quality_estimator(
      globals_->network_quality_estimator.get());

  builder->set_user_agent(GetUserAgent());
  auto chrome_network_delegate = std::make_unique<ChromeNetworkDelegate>(
      extension_event_router_forwarder(), &system_enable_referrers_);
  // By default, data usage is considered off the record.
  chrome_network_delegate->set_data_use_aggregator(
      globals_->data_use_aggregator.get(),
      true /* is_data_usage_off_the_record */);
  builder->set_network_delegate(
      globals_->data_use_ascriber->CreateNetworkDelegate(
          std::move(chrome_network_delegate), GetMetricsDataUseForwarder()));
  std::unique_ptr<net::HostResolver> host_resolver(
      CreateGlobalHostResolver(net_log_));

  builder->set_ssl_config_service(GetSSLConfigService());
  builder->SetHttpAuthHandlerFactory(
      CreateDefaultAuthHandlerFactory(host_resolver.get()));

  builder->set_host_resolver(std::move(host_resolver));

  std::unique_ptr<net::CertVerifier> cert_verifier;
#if defined(OS_CHROMEOS)
  // Creates a CertVerifyProc that doesn't allow any profile-provided certs.
  cert_verifier = std::make_unique<net::CachingCertVerifier>(
      std::make_unique<net::MultiThreadedCertVerifier>(
          base::MakeRefCounted<chromeos::CertVerifyProcChromeOS>()));
#else
  cert_verifier = std::make_unique<net::CachingCertVerifier>(
      std::make_unique<net::MultiThreadedCertVerifier>(
          net::CertVerifyProc::CreateDefault()));
#endif
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  builder->SetCertVerifier(
      network::IgnoreErrorsCertVerifier::MaybeWrapCertVerifier(
          command_line, switches::kUserDataDir, std::move(cert_verifier)));
  UMA_HISTOGRAM_BOOLEAN(
      "Net.Certificate.IgnoreCertificateErrorsSPKIListPresent",
      command_line.HasSwitch(
          network::switches::kIgnoreCertificateErrorsSPKIList));

  std::unique_ptr<net::MultiLogCTVerifier> ct_verifier =
      std::make_unique<net::MultiLogCTVerifier>();
  // Add built-in logs
  ct_verifier->AddLogs(globals_->ct_logs);
  builder->set_ct_verifier(std::move(ct_verifier));

  SetUpProxyService(builder.get());

  if (!is_quic_allowed_on_init_)
    globals_->quic_disabled = true;

  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    globals_->system_request_context_owner =
        std::move(builder)->Create(std::move(network_context_params_).get(),
                                   !is_quic_allowed_on_init_, net_log_);
    globals_->system_request_context =
        globals_->system_request_context_owner.url_request_context_getter
            ->GetURLRequestContext();
  } else {
    globals_->system_network_context =
        content::GetNetworkServiceImpl()->CreateNetworkContextWithBuilder(
            std::move(network_context_request_),
            std::move(network_context_params_), std::move(builder),
            &globals_->system_request_context);
  }

#if defined(USE_NSS_CERTS)
  net::SetURLRequestContextForNSSHttpIO(globals_->system_request_context);
#endif
#if defined(OS_ANDROID) || defined(OS_FUCHSIA)
  net::SetGlobalCertNetFetcher(
      net::CreateCertNetFetcher(globals_->system_request_context));
#endif
}

metrics::UpdateUsagePrefCallbackType IOThread::GetMetricsDataUseForwarder() {
  return base::Bind(&UpdateMetricsUsagePrefsOnUIThread);
}

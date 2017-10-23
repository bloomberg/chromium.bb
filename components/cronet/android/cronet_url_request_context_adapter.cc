// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/cronet_url_request_context_adapter.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/cronet/android/cert/cert_verifier_cache_serializer.h"
#include "components/cronet/android/cert/proto/cert_verification.pb.h"
#include "components/cronet/android/cronet_library_loader.h"
#include "components/cronet/cronet_prefs_manager.h"
#include "components/cronet/histogram_manager.h"
#include "components/cronet/host_cache_persistence_manager.h"
#include "components/cronet/url_request_context_config.h"
#include "jni/CronetUrlRequestContext_jni.h"
#include "net/base/load_flags.h"
#include "net/base/logging_network_change_observer.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate_impl.h"
#include "net/base/url_util.h"
#include "net/cert/caching_cert_verifier.h"
#include "net/cert/cert_verifier.h"
#include "net/cookies/cookie_monster.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/log/file_net_log_observer.h"
#include "net/log/net_log_util.h"
#include "net/nqe/external_estimate_provider.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/proxy/proxy_config_service_android.h"
#include "net/proxy/proxy_service.h"
#include "net/quic/core/quic_versions.h"
#include "net/ssl/channel_id_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_interceptor.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

// This class wraps a NetLog that also contains network change events.
class NetLogWithNetworkChangeEvents {
 public:
  NetLogWithNetworkChangeEvents() {}

  net::NetLog* net_log() { return &net_log_; }
  // This function registers with the NetworkChangeNotifier and so must be
  // called *after* the NetworkChangeNotifier is created. Should only be
  // called on the init thread as it is not thread-safe and the init thread is
  // the thread the NetworkChangeNotifier is created on. This function is
  // not thread-safe because accesses to |net_change_logger_| are not atomic.
  // There might be multiple CronetEngines each with a network thread so
  // so the init thread is used. |g_net_log_| also outlives the network threads
  // so it would be unsafe to receive callbacks on the network threads without
  // a complicated thread-safe reference-counting system to control callback
  // registration.
  void EnsureInitializedOnInitThread() {
    DCHECK(cronet::OnInitThread());
    if (net_change_logger_)
      return;
    net_change_logger_.reset(new net::LoggingNetworkChangeObserver(&net_log_));
  }

 private:
  net::NetLog net_log_;
  // LoggingNetworkChangeObserver logs network change events to a NetLog.
  // This class bundles one LoggingNetworkChangeObserver with one NetLog,
  // so network change event are logged just once in the NetLog.
  std::unique_ptr<net::LoggingNetworkChangeObserver> net_change_logger_;

  DISALLOW_COPY_AND_ASSIGN(NetLogWithNetworkChangeEvents);
};

// Use a global NetLog instance. See crbug.com/486120.
static base::LazyInstance<NetLogWithNetworkChangeEvents>::Leaky g_net_log =
    LAZY_INSTANCE_INITIALIZER;

class BasicNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  BasicNetworkDelegate() {}
  ~BasicNetworkDelegate() override {}

 private:
  // net::NetworkDelegate implementation.
  int OnBeforeURLRequest(net::URLRequest* request,
                         const net::CompletionCallback& callback,
                         GURL* new_url) override {
    return net::OK;
  }

  int OnBeforeStartTransaction(net::URLRequest* request,
                               const net::CompletionCallback& callback,
                               net::HttpRequestHeaders* headers) override {
    return net::OK;
  }

  void OnStartTransaction(net::URLRequest* request,
                          const net::HttpRequestHeaders& headers) override {}

  int OnHeadersReceived(
      net::URLRequest* request,
      const net::CompletionCallback& callback,
      const net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* _response_headers,
      GURL* allowed_unsafe_redirect_url) override {
    return net::OK;
  }

  void OnBeforeRedirect(net::URLRequest* request,
                        const GURL& new_location) override {}

  void OnResponseStarted(net::URLRequest* request, int net_error) override {}

  void OnCompleted(net::URLRequest* request,
                   bool started,
                   int net_error) override {}

  void OnURLRequestDestroyed(net::URLRequest* request) override {}

  void OnPACScriptError(int line_number, const base::string16& error) override {
  }

  NetworkDelegate::AuthRequiredResponse OnAuthRequired(
      net::URLRequest* request,
      const net::AuthChallengeInfo& auth_info,
      const AuthCallback& callback,
      net::AuthCredentials* credentials) override {
    return net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;
  }

  bool OnCanGetCookies(const net::URLRequest& request,
                       const net::CookieList& cookie_list) override {
    // Disallow sending cookies by default.
    return false;
  }

  bool OnCanSetCookie(const net::URLRequest& request,
                      const std::string& cookie_line,
                      net::CookieOptions* options) override {
    // Disallow saving cookies by default.
    return false;
  }

  bool OnCanAccessFile(const net::URLRequest& request,
                       const base::FilePath& original_path,
                       const base::FilePath& absolute_path) const override {
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(BasicNetworkDelegate);
};

// Helper method that takes a Java string that can be null, in which case it
// will get converted to an empty string.
std::string ConvertNullableJavaStringToUTF8(JNIEnv* env,
                                            const JavaParamRef<jstring>& jstr) {
  std::string str;
  if (!jstr.is_null())
    base::android::ConvertJavaStringToUTF8(env, jstr, &str);
  return str;
}

}  // namespace

namespace cronet {

CronetURLRequestContextAdapter::CronetURLRequestContextAdapter(
    std::unique_ptr<URLRequestContextConfig> context_config)
    : network_thread_(new base::Thread("network")),
      context_config_(std::move(context_config)),
      is_context_initialized_(false),
      default_load_flags_(net::LOAD_NORMAL) {
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  network_thread_->StartWithOptions(options);
}

CronetURLRequestContextAdapter::~CronetURLRequestContextAdapter() {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());

  if (cronet_prefs_manager_)
    cronet_prefs_manager_->PrepareForShutdown();

  if (network_quality_estimator_) {
    network_quality_estimator_->RemoveRTTObserver(this);
    network_quality_estimator_->RemoveThroughputObserver(this);
    network_quality_estimator_->RemoveEffectiveConnectionTypeObserver(this);
    network_quality_estimator_->RemoveRTTAndThroughputEstimatesObserver(this);
  }

  // Stop NetLog observer if there is one.
  StopNetLogOnNetworkThread();
}

void CronetURLRequestContextAdapter::InitRequestContextOnInitThread(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  base::android::ScopedJavaGlobalRef<jobject> jcaller_ref;
  jcaller_ref.Reset(env, jcaller);
  proxy_config_service_ =
      net::ProxyService::CreateSystemProxyConfigService(GetNetworkTaskRunner());
  net::ProxyConfigServiceAndroid* android_proxy_config_service =
      static_cast<net::ProxyConfigServiceAndroid*>(proxy_config_service_.get());
  // If a PAC URL is present, ignore it and use the address and port of
  // Android system's local HTTP proxy server. See: crbug.com/432539.
  // TODO(csharrison) Architect the wrapper better so we don't need to cast for
  // android ProxyConfigServices.
  android_proxy_config_service->set_exclude_pac_url(true);
  g_net_log.Get().EnsureInitializedOnInitThread();
  GetNetworkTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&CronetURLRequestContextAdapter::InitializeOnNetworkThread,
                 base::Unretained(this), base::Passed(&context_config_),
                 jcaller_ref));
}

void CronetURLRequestContextAdapter::
    ConfigureNetworkQualityEstimatorOnNetworkThreadForTesting(
        bool use_local_host_requests,
        bool use_smaller_responses,
        bool disable_offline_check) {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());
  network_quality_estimator_->SetUseLocalHostRequestsForTesting(
      use_local_host_requests);
  network_quality_estimator_->SetUseSmallResponsesForTesting(
      use_smaller_responses);
  network_quality_estimator_->DisableOfflineCheckForTesting(
      disable_offline_check);
}

void CronetURLRequestContextAdapter::ConfigureNetworkQualityEstimatorForTesting(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jboolean use_local_host_requests,
    jboolean use_smaller_responses,
    jboolean disable_offline_check) {
  PostTaskToNetworkThread(
      FROM_HERE,
      base::Bind(&CronetURLRequestContextAdapter::
                     ConfigureNetworkQualityEstimatorOnNetworkThreadForTesting,
                 base::Unretained(this), use_local_host_requests,
                 use_smaller_responses, disable_offline_check));
}

void CronetURLRequestContextAdapter::ProvideRTTObservationsOnNetworkThread(
    bool should) {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());
  if (!network_quality_estimator_)
    return;
  if (should) {
    network_quality_estimator_->AddRTTObserver(this);
  } else {
    network_quality_estimator_->RemoveRTTObserver(this);
  }
}

void CronetURLRequestContextAdapter::ProvideRTTObservations(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    bool should) {
  PostTaskToNetworkThread(FROM_HERE,
                          base::Bind(&CronetURLRequestContextAdapter::
                                         ProvideRTTObservationsOnNetworkThread,
                                     base::Unretained(this), should));
}

void CronetURLRequestContextAdapter::
    ProvideThroughputObservationsOnNetworkThread(bool should) {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());
  if (!network_quality_estimator_)
    return;
  if (should) {
    network_quality_estimator_->AddThroughputObserver(this);
  } else {
    network_quality_estimator_->RemoveThroughputObserver(this);
  }
}

void CronetURLRequestContextAdapter::ProvideThroughputObservations(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    bool should) {
  PostTaskToNetworkThread(
      FROM_HERE, base::Bind(&CronetURLRequestContextAdapter::
                                ProvideThroughputObservationsOnNetworkThread,
                            base::Unretained(this), should));
}

void CronetURLRequestContextAdapter::InitializeNQEPrefsOnNetworkThread() const {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());

  // Initializing |network_qualities_prefs_manager_| may post a callback to
  // |this|. So, |network_qualities_prefs_manager_| should be initialized after
  // |jcronet_url_request_context_| has been constructed.
  DCHECK(jcronet_url_request_context_.obj());
  cronet_prefs_manager_->SetupNqePersistence(network_quality_estimator_.get());
}

void CronetURLRequestContextAdapter::InitializeOnNetworkThread(
    std::unique_ptr<URLRequestContextConfig> config,
    const base::android::ScopedJavaGlobalRef<jobject>&
        jcronet_url_request_context) {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());
  DCHECK(!is_context_initialized_);
  DCHECK(proxy_config_service_);

  base::DisallowBlocking();

  // TODO(mmenke):  Add method to have the builder enable SPDY.
  net::URLRequestContextBuilder context_builder;

  std::unique_ptr<net::NetworkDelegate> network_delegate(
      new BasicNetworkDelegate());
  context_builder.set_network_delegate(std::move(network_delegate));
  context_builder.set_net_log(g_net_log.Get().net_log());

  // Android provides a local HTTP proxy server that handles proxying when a PAC
  // URL is present. Create a proxy service without a resolver and rely on this
  // local HTTP proxy. See: crbug.com/432539.
  context_builder.set_proxy_service(
      net::ProxyService::CreateWithoutProxyResolver(
          std::move(proxy_config_service_), g_net_log.Get().net_log()));

  config->ConfigureURLRequestContextBuilder(&context_builder,
                                            g_net_log.Get().net_log());

  effective_experimental_options_ =
      std::move(config->effective_experimental_options);

  if (config->enable_network_quality_estimator) {
    DCHECK(!network_quality_estimator_);
    std::unique_ptr<net::NetworkQualityEstimatorParams> nqe_params =
        base::MakeUnique<net::NetworkQualityEstimatorParams>(
            std::map<std::string, std::string>());
    if (config->nqe_forced_effective_connection_type) {
      nqe_params->SetForcedEffectiveConnectionType(
          config->nqe_forced_effective_connection_type.value());
    }

    network_quality_estimator_ = base::MakeUnique<net::NetworkQualityEstimator>(
        std::unique_ptr<net::ExternalEstimateProvider>(), std::move(nqe_params),
        g_net_log.Get().net_log());
    network_quality_estimator_->AddEffectiveConnectionTypeObserver(this);
    network_quality_estimator_->AddRTTAndThroughputEstimatesObserver(this);

    context_builder.set_network_quality_estimator(
        network_quality_estimator_.get());
  }

  DCHECK(!cronet_prefs_manager_);

  // Set up pref file if storage path is specified.
  if (!config->storage_path.empty()) {
    base::FilePath storage_path(config->storage_path);
    // Set up the HttpServerPropertiesManager.
    cronet_prefs_manager_ = std::make_unique<CronetPrefsManager>(
        config->storage_path, GetNetworkTaskRunner(),
        GetFileThread()->task_runner(),
        config->enable_network_quality_estimator,
        config->enable_host_cache_persistence, g_net_log.Get().net_log(),
        &context_builder);
  }

  // Explicitly disable the persister for Cronet to avoid persistence of dynamic
  // HPKP. This is a safety measure ensuring that nobody enables the persistence
  // of HPKP by specifying transport_security_persister_path in the future.
  context_builder.set_transport_security_persister_path(base::FilePath());

  // Disable net::CookieStore and net::ChannelIDService.
  context_builder.SetCookieAndChannelIdStores(nullptr, nullptr);

  context_ = context_builder.Build();

  // Set up host cache persistence if it's enabled. Happens after building the
  // URLRequestContext to get access to the HostCache.
  if (config->enable_host_cache_persistence && cronet_prefs_manager_) {
    net::HostCache* host_cache = context_->host_resolver()->GetHostCache();
    cronet_prefs_manager_->SetupHostCachePersistence(
        host_cache, config->host_cache_persistence_delay_ms,
        g_net_log.Get().net_log());
  }

  context_->set_check_cleartext_permitted(true);
  context_->set_enable_brotli(config->enable_brotli);

  if (config->load_disable_cache)
    default_load_flags_ |= net::LOAD_DISABLE_CACHE;

  if (config->enable_quic) {
    for (const auto& quic_hint : config->quic_hints) {
      if (quic_hint->host.empty()) {
        LOG(ERROR) << "Empty QUIC hint host: " << quic_hint->host;
        continue;
      }

      url::CanonHostInfo host_info;
      std::string canon_host(
          net::CanonicalizeHost(quic_hint->host, &host_info));
      if (!host_info.IsIPAddress() &&
          !net::IsCanonicalizedHostCompliant(canon_host)) {
        LOG(ERROR) << "Invalid QUIC hint host: " << quic_hint->host;
        continue;
      }

      if (quic_hint->port <= std::numeric_limits<uint16_t>::min() ||
          quic_hint->port > std::numeric_limits<uint16_t>::max()) {
        LOG(ERROR) << "Invalid QUIC hint port: " << quic_hint->port;
        continue;
      }

      if (quic_hint->alternate_port <= std::numeric_limits<uint16_t>::min() ||
          quic_hint->alternate_port > std::numeric_limits<uint16_t>::max()) {
        LOG(ERROR) << "Invalid QUIC hint alternate port: "
                   << quic_hint->alternate_port;
        continue;
      }

      url::SchemeHostPort quic_server("https", canon_host, quic_hint->port);
      net::AlternativeService alternative_service(
          net::kProtoQUIC, "",
          static_cast<uint16_t>(quic_hint->alternate_port));
      context_->http_server_properties()->SetQuicAlternativeService(
          quic_server, alternative_service, base::Time::Max(),
          net::QuicTransportVersionVector());
    }
  }

  // If there is a cert_verifier, then populate its cache with
  // |cert_verifier_data|.
  if (!config->cert_verifier_data.empty() && context_->cert_verifier()) {
    SCOPED_UMA_HISTOGRAM_TIMER("Net.Cronet.CertVerifierCache.DeserializeTime");
    std::string data;
    cronet_pb::CertVerificationCache cert_verification_cache;
    if (base::Base64Decode(config->cert_verifier_data, &data) &&
        cert_verification_cache.ParseFromString(data)) {
      DeserializeCertVerifierCache(cert_verification_cache,
                                   reinterpret_cast<net::CachingCertVerifier*>(
                                       context_->cert_verifier()));
    }
  }

  // Iterate through PKP configuration for every host.
  for (const auto& pkp : config->pkp_list) {
    // Add the host pinning.
    context_->transport_security_state()->AddHPKP(
        pkp->host, pkp->expiration_date, pkp->include_subdomains,
        pkp->pin_hashes, GURL::EmptyGURL());
  }

  context_->transport_security_state()
      ->SetEnablePublicKeyPinningBypassForLocalTrustAnchors(
          config->bypass_public_key_pinning_for_local_trust_anchors);

  JNIEnv* env = base::android::AttachCurrentThread();
  jcronet_url_request_context_.Reset(env, jcronet_url_request_context.obj());
  Java_CronetUrlRequestContext_initNetworkThread(env,
                                                 jcronet_url_request_context);

  is_context_initialized_ = true;

  // Set up network quality prefs.
  if (config->enable_network_quality_estimator && cronet_prefs_manager_) {
    // TODO(crbug.com/758401): execute the content of
    // InitializeNQEPrefsOnNetworkThread method directly (i.e. without posting)
    // after the bug has been fixed.
    PostTaskToNetworkThread(
        FROM_HERE,
        base::Bind(
            &CronetURLRequestContextAdapter::InitializeNQEPrefsOnNetworkThread,
            base::Unretained(this)));
  }

  while (!tasks_waiting_for_context_.empty()) {
    tasks_waiting_for_context_.front().Run();
    tasks_waiting_for_context_.pop();
  }
}

void CronetURLRequestContextAdapter::Destroy(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  DCHECK(!GetNetworkTaskRunner()->BelongsToCurrentThread());
  // Stick network_thread_ in a local, as |this| may be destroyed from the
  // network thread before delete network_thread is called.
  base::Thread* network_thread = network_thread_;
  // Transfer ownership of |file_thread_| to local variable |file_thread|, so
  // the underlying thread object is not deleted when |this| is destroyed.
  base::Thread* file_thread = file_thread_.release();
  GetNetworkTaskRunner()->DeleteSoon(FROM_HERE, this);
  // Deleting thread stops it after all tasks are completed.
  delete network_thread;
  delete file_thread;
}

net::URLRequestContext* CronetURLRequestContextAdapter::GetURLRequestContext() {
  if (!context_) {
    LOG(ERROR) << "URLRequestContext is not set up";
  }
  return context_.get();
}

void CronetURLRequestContextAdapter::PostTaskToNetworkThread(
    const base::Location& posted_from,
    const base::Closure& callback) {
  GetNetworkTaskRunner()->PostTask(
      posted_from, base::Bind(&CronetURLRequestContextAdapter::
                                  RunTaskAfterContextInitOnNetworkThread,
                              base::Unretained(this), callback));
}

void CronetURLRequestContextAdapter::RunTaskAfterContextInitOnNetworkThread(
    const base::Closure& task_to_run_after_context_init) {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());
  if (is_context_initialized_) {
    DCHECK(tasks_waiting_for_context_.empty());
    task_to_run_after_context_init.Run();
    return;
  }
  tasks_waiting_for_context_.push(task_to_run_after_context_init);
}

bool CronetURLRequestContextAdapter::IsOnNetworkThread() const {
  return GetNetworkTaskRunner()->BelongsToCurrentThread();
}

scoped_refptr<base::SingleThreadTaskRunner>
CronetURLRequestContextAdapter::GetNetworkTaskRunner() const {
  return network_thread_->task_runner();
}

bool CronetURLRequestContextAdapter::StartNetLogToFile(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jstring>& jfile_name,
    jboolean jlog_all) {
  base::FilePath file_path(
      base::android::ConvertJavaStringToUTF8(env, jfile_name));
  base::ScopedFILE file(base::OpenFile(file_path, "w"));
  if (!file) {
    LOG(ERROR) << "Failed to open NetLog file for writing.";
    return false;
  }
  PostTaskToNetworkThread(
      FROM_HERE,
      base::Bind(&CronetURLRequestContextAdapter::StartNetLogOnNetworkThread,
                 base::Unretained(this), file_path, jlog_all == JNI_TRUE));
  return true;
}

void CronetURLRequestContextAdapter::StartNetLogToDisk(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jstring>& jdir_name,
    jboolean jlog_all,
    jint jmax_size) {
  PostTaskToNetworkThread(
      FROM_HERE,
      base::Bind(&CronetURLRequestContextAdapter::
                     StartNetLogToBoundedFileOnNetworkThread,
                 base::Unretained(this),
                 base::android::ConvertJavaStringToUTF8(env, jdir_name),
                 jlog_all, jmax_size));
}

void CronetURLRequestContextAdapter::StopNetLog(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  DCHECK(!GetNetworkTaskRunner()->BelongsToCurrentThread());
  PostTaskToNetworkThread(
      FROM_HERE,
      base::Bind(&CronetURLRequestContextAdapter::StopNetLogOnNetworkThread,
                 base::Unretained(this)));
}

void CronetURLRequestContextAdapter::GetCertVerifierData(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  PostTaskToNetworkThread(
      FROM_HERE,
      base::Bind(
          &CronetURLRequestContextAdapter::GetCertVerifierDataOnNetworkThread,
          base::Unretained(this)));
}

void CronetURLRequestContextAdapter::GetCertVerifierDataOnNetworkThread() {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());
  std::string encoded_data;
  if (is_context_initialized_ && context_->cert_verifier()) {
    SCOPED_UMA_HISTOGRAM_TIMER("Net.Cronet.CertVerifierCache.SerializeTime");
    std::string data;
    cronet_pb::CertVerificationCache cert_cache =
        SerializeCertVerifierCache(*reinterpret_cast<net::CachingCertVerifier*>(
            context_->cert_verifier()));
    cert_cache.SerializeToString(&data);
    base::Base64Encode(data, &encoded_data);
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CronetUrlRequestContext_onGetCertVerifierData(
      env, jcronet_url_request_context_,
      base::android::ConvertUTF8ToJavaString(env, encoded_data));
}

int CronetURLRequestContextAdapter::default_load_flags() const {
  DCHECK(is_context_initialized_);
  return default_load_flags_;
}

base::Thread* CronetURLRequestContextAdapter::GetFileThread() {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());
  if (!file_thread_) {
    file_thread_.reset(new base::Thread("Network File Thread"));
    file_thread_->Start();
  }
  return file_thread_.get();
}

void CronetURLRequestContextAdapter::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType effective_connection_type) {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());
  Java_CronetUrlRequestContext_onEffectiveConnectionTypeChanged(
      base::android::AttachCurrentThread(), jcronet_url_request_context_,
      effective_connection_type);
}

void CronetURLRequestContextAdapter::OnRTTOrThroughputEstimatesComputed(
    base::TimeDelta http_rtt,
    base::TimeDelta transport_rtt,
    int32_t downstream_throughput_kbps) {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());

  int32_t http_rtt_ms = http_rtt.InMilliseconds() <= INT32_MAX
                            ? static_cast<int32_t>(http_rtt.InMilliseconds())
                            : INT32_MAX;
  int32_t transport_rtt_ms =
      transport_rtt.InMilliseconds() <= INT32_MAX
          ? static_cast<int32_t>(transport_rtt.InMilliseconds())
          : INT32_MAX;

  Java_CronetUrlRequestContext_onRTTOrThroughputEstimatesComputed(
      base::android::AttachCurrentThread(), jcronet_url_request_context_,
      http_rtt_ms, transport_rtt_ms, downstream_throughput_kbps);
}

void CronetURLRequestContextAdapter::OnRTTObservation(
    int32_t rtt_ms,
    const base::TimeTicks& timestamp,
    net::NetworkQualityObservationSource source) {
  Java_CronetUrlRequestContext_onRttObservation(
      base::android::AttachCurrentThread(), jcronet_url_request_context_,
      rtt_ms, (timestamp - base::TimeTicks::UnixEpoch()).InMilliseconds(),
      source);
}

void CronetURLRequestContextAdapter::OnThroughputObservation(
    int32_t throughput_kbps,
    const base::TimeTicks& timestamp,
    net::NetworkQualityObservationSource source) {
  Java_CronetUrlRequestContext_onThroughputObservation(
      base::android::AttachCurrentThread(), jcronet_url_request_context_,
      throughput_kbps,
      (timestamp - base::TimeTicks::UnixEpoch()).InMilliseconds(), source);
}

void CronetURLRequestContextAdapter::StartNetLogOnNetworkThread(
    const base::FilePath& file_path,
    bool include_socket_bytes) {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());

  // Do nothing if already logging to a file.
  if (net_log_file_observer_)
    return;
  net_log_file_observer_ = net::FileNetLogObserver::CreateUnbounded(
      file_path, /*constants=*/nullptr);
  CreateNetLogEntriesForActiveObjects({context_.get()},
                                      net_log_file_observer_.get());
  net::NetLogCaptureMode capture_mode =
      include_socket_bytes ? net::NetLogCaptureMode::IncludeSocketBytes()
                           : net::NetLogCaptureMode::Default();
  net_log_file_observer_->StartObserving(g_net_log.Get().net_log(),
                                         capture_mode);
}

void CronetURLRequestContextAdapter::StartNetLogToBoundedFileOnNetworkThread(
    const std::string& dir_path,
    bool include_socket_bytes,
    int size) {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());

  // Do nothing if already logging to a directory.
  if (net_log_file_observer_)
    return;

  // TODO(eroman): The cronet API passes a directory here. But it should now
  // just pass a file path.
  base::FilePath file_path =
      base::FilePath(dir_path).AppendASCII("netlog.json");
  {
    base::ScopedAllowBlocking allow_blocking;
    if (!base::PathIsWritable(file_path)) {
      LOG(ERROR) << "Path is not writable: " << file_path.value();
    }
  }

  net_log_file_observer_ = net::FileNetLogObserver::CreateBounded(
      file_path, size, /*constants=*/nullptr);

  CreateNetLogEntriesForActiveObjects({context_.get()},
                                      net_log_file_observer_.get());

  net::NetLogCaptureMode capture_mode =
      include_socket_bytes ? net::NetLogCaptureMode::IncludeSocketBytes()
                           : net::NetLogCaptureMode::Default();
  net_log_file_observer_->StartObserving(g_net_log.Get().net_log(),
                                         capture_mode);
}

void CronetURLRequestContextAdapter::StopNetLogOnNetworkThread() {
  DCHECK(GetNetworkTaskRunner()->BelongsToCurrentThread());

  if (!net_log_file_observer_)
    return;
  net_log_file_observer_->StopObserving(
      GetNetLogInfo(),
      base::Bind(&CronetURLRequestContextAdapter::StopNetLogCompleted,
                 base::Unretained(this)));
  net_log_file_observer_.reset();
}

void CronetURLRequestContextAdapter::StopNetLogCompleted() {
  Java_CronetUrlRequestContext_stopNetLogCompleted(
      base::android::AttachCurrentThread(), jcronet_url_request_context_);
}

std::unique_ptr<base::DictionaryValue>
CronetURLRequestContextAdapter::GetNetLogInfo() const {
  std::unique_ptr<base::DictionaryValue> net_info =
      net::GetNetInfo(context_.get(), net::NET_INFO_ALL_SOURCES);
  if (effective_experimental_options_) {
    net_info->Set("cronetExperimentalParams",
                  effective_experimental_options_->CreateDeepCopy());
  }
  return net_info;
}

// Create a URLRequestContextConfig from the given parameters.
static jlong CreateRequestContextConfig(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller,
    const JavaParamRef<jstring>& juser_agent,
    const JavaParamRef<jstring>& jstorage_path,
    jboolean jquic_enabled,
    const JavaParamRef<jstring>& jquic_default_user_agent_id,
    jboolean jhttp2_enabled,
    jboolean jbrotli_enabled,
    jboolean jdisable_cache,
    jint jhttp_cache_mode,
    jlong jhttp_cache_max_size,
    const JavaParamRef<jstring>& jexperimental_quic_connection_options,
    jlong jmock_cert_verifier,
    jboolean jenable_network_quality_estimator,
    jboolean jbypass_public_key_pinning_for_local_trust_anchors,
    const JavaParamRef<jstring>& jcert_verifier_data) {
  return reinterpret_cast<jlong>(new URLRequestContextConfig(
      jquic_enabled,
      ConvertNullableJavaStringToUTF8(env, jquic_default_user_agent_id),
      jhttp2_enabled, jbrotli_enabled,
      static_cast<URLRequestContextConfig::HttpCacheType>(jhttp_cache_mode),
      jhttp_cache_max_size, jdisable_cache,
      ConvertNullableJavaStringToUTF8(env, jstorage_path),
      ConvertNullableJavaStringToUTF8(env, juser_agent),
      ConvertNullableJavaStringToUTF8(env,
                                      jexperimental_quic_connection_options),
      base::WrapUnique(
          reinterpret_cast<net::CertVerifier*>(jmock_cert_verifier)),
      jenable_network_quality_estimator,
      jbypass_public_key_pinning_for_local_trust_anchors,
      ConvertNullableJavaStringToUTF8(env, jcert_verifier_data)));
}

// Add a QUIC hint to a URLRequestContextConfig.
static void AddQuicHint(JNIEnv* env,
                        const JavaParamRef<jclass>& jcaller,
                        jlong jurl_request_context_config,
                        const JavaParamRef<jstring>& jhost,
                        jint jport,
                        jint jalternate_port) {
  URLRequestContextConfig* config =
      reinterpret_cast<URLRequestContextConfig*>(jurl_request_context_config);
  config->quic_hints.push_back(
      base::MakeUnique<URLRequestContextConfig::QuicHint>(
          base::android::ConvertJavaStringToUTF8(env, jhost), jport,
          jalternate_port));
}

// Add a public key pin to URLRequestContextConfig.
// |jhost| is the host to apply the pin to.
// |jhashes| is an array of jbyte[32] representing SHA256 key hashes.
// |jinclude_subdomains| indicates if pin should be applied to subdomains.
// |jexpiration_time| is the time that the pin expires, in milliseconds since
// Jan. 1, 1970, midnight GMT.
static void AddPkp(JNIEnv* env,
                   const JavaParamRef<jclass>& jcaller,
                   jlong jurl_request_context_config,
                   const JavaParamRef<jstring>& jhost,
                   const JavaParamRef<jobjectArray>& jhashes,
                   jboolean jinclude_subdomains,
                   jlong jexpiration_time) {
  URLRequestContextConfig* config =
      reinterpret_cast<URLRequestContextConfig*>(jurl_request_context_config);
  std::unique_ptr<URLRequestContextConfig::Pkp> pkp(
      new URLRequestContextConfig::Pkp(
          base::android::ConvertJavaStringToUTF8(env, jhost),
          jinclude_subdomains,
          base::Time::UnixEpoch() +
              base::TimeDelta::FromMilliseconds(jexpiration_time)));
  size_t hash_count = env->GetArrayLength(jhashes);
  for (size_t i = 0; i < hash_count; ++i) {
    ScopedJavaLocalRef<jbyteArray> bytes_array(
        env, static_cast<jbyteArray>(env->GetObjectArrayElement(jhashes, i)));
    static_assert(std::is_pod<net::SHA256HashValue>::value,
                  "net::SHA256HashValue is not POD");
    static_assert(sizeof(net::SHA256HashValue) * CHAR_BIT == 256,
                  "net::SHA256HashValue contains overhead");
    if (env->GetArrayLength(bytes_array.obj()) !=
        sizeof(net::SHA256HashValue)) {
      LOG(ERROR) << "Unable to add public key hash value.";
      continue;
    }
    jbyte* bytes = env->GetByteArrayElements(bytes_array.obj(), nullptr);
    net::HashValue hash(*reinterpret_cast<net::SHA256HashValue*>(bytes));
    pkp->pin_hashes.push_back(hash);
    env->ReleaseByteArrayElements(bytes_array.obj(), bytes, JNI_ABORT);
  }
  config->pkp_list.push_back(std::move(pkp));
}

// Creates RequestContextAdater if config is valid URLRequestContextConfig,
// returns 0 otherwise.
static jlong CreateRequestContextAdapter(JNIEnv* env,
                                         const JavaParamRef<jclass>& jcaller,
                                         jlong jconfig) {
  std::unique_ptr<URLRequestContextConfig> context_config(
      reinterpret_cast<URLRequestContextConfig*>(jconfig));

  CronetURLRequestContextAdapter* context_adapter =
      new CronetURLRequestContextAdapter(std::move(context_config));
  return reinterpret_cast<jlong>(context_adapter);
}

static jint SetMinLogLevel(JNIEnv* env,
                           const JavaParamRef<jclass>& jcaller,
                           jint jlog_level) {
  jint old_log_level = static_cast<jint>(logging::GetMinLogLevel());
  // MinLogLevel is global, shared by all URLRequestContexts.
  logging::SetMinLogLevel(static_cast<int>(jlog_level));
  return old_log_level;
}

static ScopedJavaLocalRef<jbyteArray> GetHistogramDeltas(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller) {
  DCHECK(base::StatisticsRecorder::IsActive());
  std::vector<uint8_t> data;
  if (!HistogramManager::GetInstance()->GetDeltas(&data))
    return ScopedJavaLocalRef<jbyteArray>();
  return base::android::ToJavaByteArray(env, &data[0], data.size());
}

}  // namespace cronet

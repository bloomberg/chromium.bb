// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_metrics_service_client.h"

#include <jni.h>
#include <cstdint>
#include <memory>

#include "android_webview/browser/metrics/aw_metrics_log_uploader.h"
#include "android_webview/browser_jni_headers/AwMetricsServiceClient_jni.h"
#include "android_webview/common/aw_features.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/hash/hash.h"
#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string16.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "components/metrics/cpu_metrics_provider.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/gpu/gpu_metrics_provider.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/net/cellular_logic_helper.h"
#include "components/metrics/net/network_metrics_provider.h"
#include "components/metrics/ui/screen_info_metrics_provider.h"
#include "components/metrics/version_utils.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/android/channel_getter.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/network_service_instance.h"

namespace android_webview {

base::LazyInstance<AwMetricsServiceClient>::Leaky g_lazy_instance_;

namespace {

// IMPORTANT: DO NOT CHANGE sample rates without first ensuring the Chrome
// Metrics team has the appropriate backend bandwidth and storage.

// Sample at 2%, based on storage concerns. We sample at a different rate than
// Chrome because we have more metrics "clients" (each app on the device counts
// as a separate client).
const double kStableSampledInRate = 0.02;

// Sample non-stable channels also at 2%. We intend to raise this to 99% in the
// future (for consistency with Chrome and to exercise the out-of-sample code
// path).
const double kBetaDevCanarySampledInRate = 0.02;

// Callbacks for metrics::MetricsStateManager::Create. Store/LoadClientInfo
// allow Windows Chrome to back up ClientInfo. They're no-ops for WebView.

void StoreClientInfo(const metrics::ClientInfo& client_info) {}

std::unique_ptr<metrics::ClientInfo> LoadClientInfo() {
  std::unique_ptr<metrics::ClientInfo> client_info;
  return client_info;
}

// WebView metrics are sampled at (possibly) different rates depending on
// channel, based on the client ID. Sampling is hard-coded (rather than
// controlled via variations, as in Chrome) because:
// - WebView is slow to download the variations seed and propagate it to each
//   app, so we'd miss metrics from the first few runs of each app.
// - WebView uses the low-entropy source for all studies, so there would be
//   crosstalk between the metrics sampling study and all other studies.
bool IsInSample(const std::string& client_id) {
  DCHECK(!client_id.empty());

  double sampled_in_rate = kBetaDevCanarySampledInRate;

  // Down-sample unknown channel as a precaution in case it ends up being
  // shipped to Stable users.
  version_info::Channel channel = version_info::android::GetChannel();
  if (channel == version_info::Channel::STABLE ||
      channel == version_info::Channel::UNKNOWN) {
    sampled_in_rate = kStableSampledInRate;
  }

  // client_id comes from base::GenerateGUID(), so its value is random/uniform,
  // except for a few bit positions with fixed values, and some hyphens. Rather
  // than separating the random payload from the fixed bits, just hash the whole
  // thing, to produce a new random/~uniform value.
  uint32_t hash = base::PersistentHash(client_id);

  // Since hashing is ~uniform, the chance that the value falls in the bottom
  // X% of possible values is X%. UINT32_MAX fits within the range of integers
  // that can be expressed precisely by a 64-bit double. Casting back to a
  // uint32_t means the effective sample rate is within a 1/UINT32_MAX error
  // margin.
  uint32_t sampled_in_threshold =
      static_cast<uint32_t>(static_cast<double>(UINT32_MAX) * sampled_in_rate);
  return hash < sampled_in_threshold;
}

std::unique_ptr<metrics::MetricsService> CreateMetricsService(
    metrics::MetricsStateManager* state_manager,
    metrics::MetricsServiceClient* client,
    PrefService* prefs) {
  auto service =
      std::make_unique<metrics::MetricsService>(state_manager, client, prefs);
  service->RegisterMetricsProvider(
      std::make_unique<metrics::NetworkMetricsProvider>(
          content::CreateNetworkConnectionTrackerAsyncGetter()));
  service->RegisterMetricsProvider(
      std::make_unique<metrics::CPUMetricsProvider>());
  service->RegisterMetricsProvider(
      std::make_unique<metrics::GPUMetricsProvider>());
  service->RegisterMetricsProvider(
      std::make_unique<metrics::ScreenInfoMetricsProvider>());
  service->RegisterMetricsProvider(
      std::make_unique<metrics::CallStackProfileMetricsProvider>());
  service->InitializeMetricsRecordingState();
  return service;
}

// Queries the system for the app's first install time and uses this in the
// kInstallDate pref. Must be called before created a MetricsStateManager.
// TODO(https://crbug.com/1012025): remove this when the kInstallDate pref has
// been persisted for one or two milestones.
void PopulateSystemInstallDateIfNecessary(PrefService* prefs) {
  int64_t install_date = prefs->GetInt64(metrics::prefs::kInstallDate);
  if (install_date > 0) {
    // kInstallDate appears to be valid (common case). Finish early as an
    // optimization to avoid a JNI call below.
    base::UmaHistogramEnumeration("Android.WebView.Metrics.BackfillInstallDate",
                                  BackfillInstallDate::kValidInstallDatePref);
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  int64_t system_install_date =
      Java_AwMetricsServiceClient_getAppInstallTime(env);
  if (system_install_date < 0) {
    // Could not figure out install date from the system. Let the
    // MetricsStateManager set this pref to its best guess for a reasonable
    // time.
    base::UmaHistogramEnumeration(
        "Android.WebView.Metrics.BackfillInstallDate",
        BackfillInstallDate::kCouldNotGetPackageManagerInstallDate);
    return;
  }

  base::UmaHistogramEnumeration(
      "Android.WebView.Metrics.BackfillInstallDate",
      BackfillInstallDate::kPersistedPackageManagerInstallDate);
  prefs->SetInt64(metrics::prefs::kInstallDate, system_install_date);
}

}  // namespace

// static
AwMetricsServiceClient* AwMetricsServiceClient::GetInstance() {
  AwMetricsServiceClient* client = g_lazy_instance_.Pointer();
  DCHECK_CALLED_ON_VALID_SEQUENCE(client->sequence_checker_);
  return client;
}

AwMetricsServiceClient::AwMetricsServiceClient() {}
AwMetricsServiceClient::~AwMetricsServiceClient() {}

void AwMetricsServiceClient::Initialize(PrefService* pref_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!init_finished_);

  pref_service_ = pref_service;

  PopulateSystemInstallDateIfNecessary(pref_service_);
  metrics_state_manager_ = metrics::MetricsStateManager::Create(
      pref_service_, this, base::string16(),
      base::BindRepeating(&StoreClientInfo),
      base::BindRepeating(&LoadClientInfo));

  init_finished_ = true;
  MaybeStartMetrics();
}

void AwMetricsServiceClient::MaybeStartMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Treat the debugging flag the same as user consent because the user set it,
  // but keep app_consent_ separate so we never persist data from an opted-out
  // app.
  bool user_consent_or_flag = user_consent_ || IsMetricsReportingForceEnabled();
  if (init_finished_ && set_consent_finished_) {
    if (app_consent_ && user_consent_or_flag) {
      metrics_service_ = CreateMetricsService(metrics_state_manager_.get(),
                                              this, pref_service_);
      metrics_state_manager_->ForceClientIdCreation();
      is_in_sample_ = IsInSample();
      if (IsReportingEnabled()) {
        // WebView has no shutdown sequence, so there's no need for a matching
        // Stop() call.
        metrics_service_->Start();
      }
    } else {
      pref_service_->ClearPref(metrics::prefs::kMetricsClientID);
    }
  }
}

void AwMetricsServiceClient::SetHaveMetricsConsent(bool user_consent,
                                                   bool app_consent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  set_consent_finished_ = true;
  user_consent_ = user_consent;
  app_consent_ = app_consent;
  MaybeStartMetrics();
}

std::unique_ptr<const base::FieldTrial::EntropyProvider>
AwMetricsServiceClient::CreateLowEntropyProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return metrics_state_manager_->CreateLowEntropyProvider();
}

bool AwMetricsServiceClient::IsConsentGiven() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return user_consent_ && app_consent_;
}

bool AwMetricsServiceClient::IsReportingEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!app_consent_)
    return false;
  return IsMetricsReportingForceEnabled() ||
         (EnabledStateProvider::IsReportingEnabled() && is_in_sample_);
}

metrics::MetricsService* AwMetricsServiceClient::GetMetricsService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This will be null if initialization hasn't finished, or if metrics
  // collection is disabled.
  return metrics_service_.get();
}

// In Chrome, UMA and Breakpad are enabled/disabled together by the same
// checkbox and they share the same client ID (a.k.a. GUID). SetMetricsClientId
// is intended to provide the ID to Breakpad. In WebView, UMA and Breakpad are
// independent, so this is a no-op.
void AwMetricsServiceClient::SetMetricsClientId(const std::string& client_id) {}

int32_t AwMetricsServiceClient::GetProduct() {
  return metrics::ChromeUserMetricsExtension::ANDROID_WEBVIEW;
}

std::string AwMetricsServiceClient::GetApplicationLocale() {
  return base::i18n::GetConfiguredLocale();
}

bool AwMetricsServiceClient::GetBrand(std::string* brand_code) {
  // WebView doesn't use brand codes.
  return false;
}

metrics::SystemProfileProto::Channel AwMetricsServiceClient::GetChannel() {
  return metrics::AsProtobufChannel(version_info::android::GetChannel());
}

std::string AwMetricsServiceClient::GetVersionString() {
  return version_info::GetVersionNumber();
}

void AwMetricsServiceClient::CollectFinalMetricsForLog(
    const base::Closure& done_callback) {
  done_callback.Run();
}

std::unique_ptr<metrics::MetricsLogUploader>
AwMetricsServiceClient::CreateUploader(
    const GURL& server_url,
    const GURL& insecure_server_url,
    base::StringPiece mime_type,
    metrics::MetricsLogUploader::MetricServiceType service_type,
    const metrics::MetricsLogUploader::UploadCallback& on_upload_complete) {
  // |server_url|, |insecure_server_url|, and |mime_type| are unused because
  // WebView sends metrics to the platform logging mechanism rather than to
  // Chrome's metrics server.
  return std::make_unique<AwMetricsLogUploader>(on_upload_complete);
}

base::TimeDelta AwMetricsServiceClient::GetStandardUploadInterval() {
  // In WebView, metrics collection (when we batch up all logged histograms into
  // a ChromeUserMetricsExtension proto) and metrics uploading (when the proto
  // goes to the server) happen separately.
  //
  // This interval controls the metrics collection rate, so we choose the
  // standard upload interval to make sure we're collecting metrics consistently
  // with Chrome for Android. The metrics uploading rate for WebView is
  // controlled by the platform logging mechanism. Since this mechanism has its
  // own logic for rate-limiting on cellular connections, we disable the
  // component-layer logic.
  return metrics::GetUploadInterval(false /* use_cellular_upload_interval */);
}

std::string AwMetricsServiceClient::GetAppPackageName() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_app_name =
      Java_AwMetricsServiceClient_getAppPackageName(env);
  if (j_app_name)
    return ConvertJavaStringToUTF8(env, j_app_name);
  return std::string();
}

bool AwMetricsServiceClient::IsInSample() {
  // Called in MaybeStartMetrics(), after metrics_service_ is created.
  return ::android_webview::IsInSample(metrics_service_->GetClientId());
}

// static
void JNI_AwMetricsServiceClient_SetHaveMetricsConsent(JNIEnv* env,
                                                      jboolean user_consent,
                                                      jboolean app_consent) {
  AwMetricsServiceClient::GetInstance()->SetHaveMetricsConsent(user_consent,
                                                               app_consent);
}

}  // namespace android_webview

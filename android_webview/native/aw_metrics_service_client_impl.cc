// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_metrics_service_client_impl.h"

#include "android_webview/common/aw_version_info_values.h"
#include "android_webview/jni/AwMetricsServiceClient_jni.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/i18n/rtl.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/gpu/gpu_metrics_provider.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/net/net_metrics_log_uploader.h"
#include "components/metrics/profiler/profiler_metrics_provider.h"
#include "components/metrics/ui/screen_info_metrics_provider.h"
#include "components/metrics/url_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace android_webview {

base::LazyInstance<AwMetricsServiceClientImpl>::Leaky g_lazy_instance_;

namespace {

const int kUploadIntervalMinutes = 30;

// Callbacks for metrics::MetricsStateManager::Create. Store/LoadClientInfo
// allow Windows Chrome to back up ClientInfo. They're no-ops for WebView.

void StoreClientInfo(const metrics::ClientInfo& client_info) {}

std::unique_ptr<metrics::ClientInfo> LoadClientInfo() {
  std::unique_ptr<metrics::ClientInfo> client_info;
  return client_info;
}

// A GUID in text form is composed of 32 hex digits and 4 hyphens.
const size_t GUID_SIZE = 32 + 4;

void GetOrCreateGUID(const base::FilePath guid_file_path, std::string* guid) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);

  // Try to read an existing GUID.
  if (base::ReadFileToStringWithMaxSize(guid_file_path, guid, GUID_SIZE)) {
    if (base::IsValidGUID(*guid))
      return;
    else
      LOG(ERROR) << "Overwriting invalid GUID";
  }

  // We must write a new GUID.
  *guid = base::GenerateGUID();
  if (!base::WriteFile(guid_file_path, guid->c_str(), guid->size())) {
    // If writing fails, proceed anyway with the new GUID. It won't be persisted
    // to the next run, but we can still collect metrics with this 1-time GUID.
    LOG(ERROR) << "Failed to write new GUID";
  }
  return;
}

}  // namespace

// static
AwMetricsServiceClient* AwMetricsServiceClient::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return g_lazy_instance_.Pointer();
}

void AwMetricsServiceClientImpl::Initialize(
    PrefService* pref_service,
    net::URLRequestContextGetter* request_context,
    const base::FilePath guid_file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK(pref_service_ == nullptr); // Initialize should only happen once.
  DCHECK(request_context_ == nullptr);
  pref_service_ = pref_service;
  request_context_ = request_context;

  std::string* guid = new std::string;
  // Initialization happens on the UI thread, but getting the GUID should happen
  // on the file I/O thread. So we start to initialize, then post to get the
  // GUID, and then pick up where we left off, back on the UI thread, in
  // InitializeWithGUID.
  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&GetOrCreateGUID, guid_file_path, guid),
      base::Bind(&AwMetricsServiceClientImpl::InitializeWithGUID,
                 base::Unretained(this), base::Owned(guid)));
}

void AwMetricsServiceClientImpl::InitializeWithGUID(std::string* guid) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  pref_service_->SetString(metrics::prefs::kMetricsClientID, *guid);

  metrics_state_manager_ = metrics::MetricsStateManager::Create(
      pref_service_, this, base::Bind(&StoreClientInfo),
      base::Bind(&LoadClientInfo));

  metrics_service_.reset(new ::metrics::MetricsService(
      metrics_state_manager_.get(), this, pref_service_));

  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new metrics::NetworkMetricsProvider(
              content::BrowserThread::GetBlockingPool())));

  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new metrics::GPUMetricsProvider));

  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new metrics::ScreenInfoMetricsProvider));

  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new metrics::ProfilerMetricsProvider()));

  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new metrics::CallStackProfileMetricsProvider));

  metrics_service_->InitializeMetricsRecordingState();

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AwMetricsServiceClient_nativeInitialized(env);
}

bool AwMetricsServiceClientImpl::IsConsentGiven() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return is_enabled_;
}

void AwMetricsServiceClientImpl::SetMetricsEnabled(bool enabled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_enabled_ != enabled) {
    if (enabled) {
      // TODO(paulmiller): Actually enable metrics when the server-side is ready
      //metrics_service_->Start();
    } else {
      metrics_service_->Stop();
    }
    is_enabled_ = enabled;
  }
}

metrics::MetricsService* AwMetricsServiceClientImpl::GetMetricsService() {
  return metrics_service_.get();
}

// In Chrome, UMA and Breakpad are enabled/disabled together by the same
// checkbox and they share the same client ID (a.k.a. GUID). SetMetricsClientId
// is intended to provide the ID to Breakpad. In WebView, UMA and Breakpad are
// independent, so this is a no-op.
void AwMetricsServiceClientImpl::SetMetricsClientId(
    const std::string& client_id) {}

int32_t AwMetricsServiceClientImpl::GetProduct() {
  return ::metrics::ChromeUserMetricsExtension::ANDROID_WEBVIEW;
}

std::string AwMetricsServiceClientImpl::GetApplicationLocale() {
  return base::i18n::GetConfiguredLocale();
}

bool AwMetricsServiceClientImpl::GetBrand(std::string* brand_code) {
  // WebView doesn't use brand codes.
  return false;
}

metrics::SystemProfileProto::Channel AwMetricsServiceClientImpl::GetChannel() {
  // "Channel" means stable, beta, etc. WebView doesn't have channel info yet.
  // TODO(paulmiller) Update this once we have channel info.
  return metrics::SystemProfileProto::CHANNEL_UNKNOWN;
}

std::string AwMetricsServiceClientImpl::GetVersionString() {
  return PRODUCT_VERSION;
}

void AwMetricsServiceClientImpl::InitializeSystemProfileMetrics(
    const base::Closure& done_callback) {
  done_callback.Run();
}

void AwMetricsServiceClientImpl::CollectFinalMetricsForLog(
    const base::Closure& done_callback) {
  done_callback.Run();
}

std::unique_ptr<metrics::MetricsLogUploader>
AwMetricsServiceClientImpl::CreateUploader(
    const std::string& server_url,
    const std::string& mime_type,
    metrics::MetricsLogUploader::MetricServiceType service_type,
    const base::Callback<void(int)>& on_upload_complete) {
  return std::unique_ptr<::metrics::MetricsLogUploader>(
      new metrics::NetMetricsLogUploader(request_context_, server_url,
                                         mime_type, service_type,
                                         on_upload_complete));
}

base::TimeDelta AwMetricsServiceClientImpl::GetStandardUploadInterval() {
  return base::TimeDelta::FromMinutes(kUploadIntervalMinutes);
}

AwMetricsServiceClientImpl::AwMetricsServiceClientImpl()
    : is_enabled_(false),
      pref_service_(nullptr),
      request_context_(nullptr) {}

AwMetricsServiceClientImpl::~AwMetricsServiceClientImpl() {}

// static
void SetMetricsEnabled(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    jboolean enabled) {
  g_lazy_instance_.Pointer()->SetMetricsEnabled(enabled);
}

bool RegisterAwMetricsServiceClient(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android_webview

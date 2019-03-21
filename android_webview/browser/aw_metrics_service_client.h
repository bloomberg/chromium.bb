// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_METRICS_SERVICE_CLIENT_H_
#define ANDROID_WEBVIEW_BROWSER_AW_METRICS_SERVICE_CLIENT_H_

#include <memory>
#include <string>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/sequence_checker.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_service_client.h"

class PrefService;

namespace base {
class FilePath;
}

namespace metrics {
class MetricsStateManager;
}

namespace android_webview {

// Exposed for testing.
namespace internal {
bool GetLegacyClientIdPath(base::FilePath* path);
}

// AwMetricsServiceClient is a singleton which manages WebView metrics
// collection.
//
// Metrics should be enabled iff all these conditions are met:
//  - The user has not opted out (controlled by GMS).
//  - The app has not opted out (controlled by manifest tag).
//  - This client is in the 2% sample (controlled by client ID hash).
// The first two are recorded in |user_and_app_consent_|, which is set by
// SetHaveMetricsConsent(). The last is recorded in |is_in_sample_|.
//
// Metrics are pseudonymously identified by a randomly-generated "client ID".
// WebView stores this in the app's data directory. There's a different such
// directory for each user, for each app, on each device. So the ID should be
// unique per (device, app, user) tuple.
//
// The client ID should be stored in prefs. But as a vestige from before WebView
// persisted prefs across runs, it may be stored in a separate file named
// "metrics_guid". If such a file is found, it should be deleted and the ID
// moved into prefs.
//
// To avoid the appearance that we're doing anything sneaky, the client ID
// should only be created or persisted when neither the user nor the app have
// opted out. Otherwise, the presence of the ID could give the impression that
// metrics were being collected.
//
// WebView metrics set up happens like so:
//
//   startup
//      │
//      ├──────────────────────────┐
//      │                          ▼
//      ▼                       query GMS for consent
//   Initialize()                  │
//      │                          │
//      ▼                          │
//   LoadLegacyClientId()          │
//      │                          │
//      ▼                          │
//   InitializeWithClientId()      ▼
//      │                       SetHaveMetricsConsent()
//      │                          │
//      │ ┌────────────────────────┘
//      ▼ ▼
//   MaybeStartMetrics()
//      │
//      ▼
//   MetricsService::Start()
//
// LoadLegacyClientId() is the only function in this diagram that happens off
// the UI thread. It checks for the legacy metrics_guid file. If it contains a
// client ID, it stores the ID in |legacy_client_id_|. Then it deletes the file.
// Once ~all clients have deleted the file, LoadLegacyClientId() can be removed,
// and Initialize() and InitializeWithClientId() can be merged.
//
// Querying GMS is slow, so SetHaveMetricsConsent() typically happens after
// InitializeWithClientId(). But it may happen before Initialize(), or between
// Initialize() and InitializeWithClientId().
//
// Each path sets a flag, |init_finished_| or |set_consent_finished_|, to show
// that path has finished, and then calls MaybeStartMetrics(). When
// MaybeStartMetrics() is called the second time, it sees both flags true,
// meaning we have both the client ID (if any) and the user/app opt-out status.
//
// If consent is granted, MaybeStartMetrics() then determines sampling by
// hashing the ID (generating a new ID if there was none), and may then enable
// metrics. Otherwise, it clears the client ID.
class AwMetricsServiceClient : public metrics::MetricsServiceClient,
                               public metrics::EnabledStateProvider {
  friend struct base::LazyInstanceTraitsBase<AwMetricsServiceClient>;

 public:
  static AwMetricsServiceClient* GetInstance();

  AwMetricsServiceClient();
  ~AwMetricsServiceClient() override;

  void Initialize(PrefService* pref_service);
  void SetHaveMetricsConsent(bool consent);
  std::unique_ptr<const base::FieldTrial::EntropyProvider>
      CreateLowEntropyProvider();

  // metrics::EnabledStateProvider
  bool IsConsentGiven() const override;
  bool IsReportingEnabled() const override;

  // metrics::MetricsServiceClient
  metrics::MetricsService* GetMetricsService() override;
  void SetMetricsClientId(const std::string& client_id) override;
  int32_t GetProduct() override;
  std::string GetApplicationLocale() override;
  bool GetBrand(std::string* brand_code) override;
  metrics::SystemProfileProto::Channel GetChannel() override;
  std::string GetVersionString() override;
  void CollectFinalMetricsForLog(const base::Closure& done_callback) override;
  std::unique_ptr<metrics::MetricsLogUploader> CreateUploader(
      const GURL& server_url,
      const GURL& insecure_server_url,
      base::StringPiece mime_type,
      metrics::MetricsLogUploader::MetricServiceType service_type,
      const metrics::MetricsLogUploader::UploadCallback& on_upload_complete)
      override;
  base::TimeDelta GetStandardUploadInterval() override;
  std::string GetAppPackageName() override;

 protected:
  // virtual for testing
  virtual void InitializeWithClientId();
  virtual bool IsInSample();

 private:
  void MaybeStartMetrics();

  // Temporarily stores a client ID loaded from the legacy file, to pass it from
  // LoadLegacyClientId() to InitializeWithClientId().
  // TODO(crbug/939002): Remove this after ~all clients have migrated the ID.
  std::unique_ptr<std::string> legacy_client_id_;

  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<metrics::MetricsService> metrics_service_;
  PrefService* pref_service_ = nullptr;
  bool init_finished_ = false;
  bool set_consent_finished_ = false;
  bool user_and_app_consent_ = false;
  bool is_in_sample_ = false;

  // AwMetricsServiceClient may be created before the UI thread is promoted to
  // BrowserThread::UI. Use |sequence_checker_| to enforce that the
  // AwMetricsServiceClient is used on a single thread.
  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(AwMetricsServiceClient);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_METRICS_SERVICE_CLIENT_H_

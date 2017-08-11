// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/chrome/today_extension/today_metrics_logger.h"

#include "base/base64.h"
#include "base/cpu.h"
#include "base/mac/bind_objc_block.h"
#import "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/metrics/delegating_provider.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/version_utils.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/value_map_pref_store.h"
#include "components/variations/active_field_trials.h"
#include "components/version_info/version_info.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/app_group/app_group_metrics.h"
#import "ios/chrome/common/app_group/app_group_metrics_client.h"
#include "ios/chrome/common/channel_info.h"

namespace {

// User default key to keep track of the current log session ID. Increased every
// time a log is created. This ID must be offset using
// app_group::AppGroupSessionID.
NSString* const kTodayExtensionMetricsSessionID = @"MetricsSessionID";

// User default key to the current log serialized. In case of an extension
// restart, this log can be written to disk for upload.
NSString* const kTodayExtensionMetricsCurrentLog = @"MetricsCurrentLog";

// Maximum age of a log.
const int kMaxLogLifeTimeInSeconds = 86400;

// A simple implementation of metrics::MetricsServiceClient.
// As logs are uploaded by Chrome application, not all methods are needed.
// Only the method needed to initialize the metrics logs are implementsd.
class TodayMetricsServiceClient : public metrics::MetricsServiceClient {
 public:
  TodayMetricsServiceClient() {}
  metrics::MetricsService* GetMetricsService() override;
  void SetMetricsClientId(const std::string& client_id) override;
  int32_t GetProduct() override;
  std::string GetApplicationLocale() override;
  bool GetBrand(std::string* brand_code) override;
  metrics::SystemProfileProto::Channel GetChannel() override;
  std::string GetVersionString() override;
  void CollectFinalMetricsForLog(const base::Closure& done_callback) override;
  std::unique_ptr<metrics::MetricsLogUploader> CreateUploader(
      base::StringPiece server_url,
      base::StringPiece mime_type,
      metrics::MetricsLogUploader::MetricServiceType service_type,
      const metrics::MetricsLogUploader::UploadCallback& on_upload_complete)
      override;
  base::TimeDelta GetStandardUploadInterval() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TodayMetricsServiceClient);
};

class TodayMetricsLog : public metrics::MetricsLog {
 public:
  // Creates a new today metrics log of the specified type.
  // TodayMetricsLog is similar to metrics::MetricsLog but allow serialization
  // of open logs.
  TodayMetricsLog(const std::string& client_id,
                  int session_id,
                  LogType log_type,
                  TodayMetricsServiceClient* client);

  // Fills |encoded_log| with the serialized protobuf representation of the
  // record. Can be called even on open log.
  void GetOpenEncodedLog(std::string* encoded_log) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(TodayMetricsLog);
};

metrics::MetricsService* TodayMetricsServiceClient::GetMetricsService() {
  NOTREACHED();
  return nullptr;
}

void TodayMetricsServiceClient::SetMetricsClientId(
    const std::string& client_id) {
  NOTREACHED();
}

int32_t TodayMetricsServiceClient::GetProduct() {
  return metrics::ChromeUserMetricsExtension::CHROME;
}

bool TodayMetricsServiceClient::GetBrand(std::string* brand_code) {
  base::scoped_nsobject<NSUserDefaults> shared_defaults(
      [[NSUserDefaults alloc] initWithSuiteName:app_group::ApplicationGroup()]);

  NSString* ns_brand_code = [shared_defaults
      stringForKey:base::SysUTF8ToNSString(app_group::kBrandCode)];
  if (!ns_brand_code)
    return false;

  *brand_code = base::SysNSStringToUTF8(ns_brand_code);
  return true;
}

metrics::SystemProfileProto::Channel TodayMetricsServiceClient::GetChannel() {
  return metrics::AsProtobufChannel(::GetChannel());
}

std::string TodayMetricsServiceClient::GetApplicationLocale() {
  return base::SysNSStringToUTF8(
      [[[NSBundle mainBundle] preferredLocalizations] objectAtIndex:0]);
}

std::string TodayMetricsServiceClient::GetVersionString() {
  return metrics::GetVersionString();
}

void TodayMetricsServiceClient::CollectFinalMetricsForLog(
    const base::Closure& done_callback) {
  NOTREACHED();
}

std::unique_ptr<metrics::MetricsLogUploader>
TodayMetricsServiceClient::CreateUploader(
    base::StringPiece server_url,
    base::StringPiece mime_type,
    metrics::MetricsLogUploader::MetricServiceType service_type,
    const metrics::MetricsLogUploader::UploadCallback& on_upload_complete) {
  NOTREACHED();
  return nullptr;
}

base::TimeDelta TodayMetricsServiceClient::GetStandardUploadInterval() {
  NOTREACHED();
  return base::TimeDelta::FromSeconds(0);
}

TodayMetricsLog::TodayMetricsLog(const std::string& client_id,
                                 int session_id,
                                 LogType log_type,
                                 TodayMetricsServiceClient* client)
    : metrics::MetricsLog(client_id, session_id, log_type, client) {}

void TodayMetricsLog::GetOpenEncodedLog(std::string* encoded_log) const {
  uma_proto()->SerializeToString(encoded_log);
}

class TodayMetricsProvider : public metrics::MetricsProvider {
 public:
  TodayMetricsProvider(int64_t enabled_date, int64_t install_date)
      : enabled_date_(enabled_date), install_date_(install_date) {}
  ~TodayMetricsProvider() override = default;

  void ProvideSystemProfileMetrics(
      metrics::SystemProfileProto* system_profile) override;

 private:
  int64_t enabled_date_;
  int64_t install_date_;

  DISALLOW_COPY_AND_ASSIGN(TodayMetricsProvider);
};

// Round a timestamp measured in seconds since epoch to one with a granularity
// of an hour. This can be used before uploaded potentially sensitive
// timestamps.
int64_t RoundSecondsToHour(int64_t time_in_seconds) {
  return 3600 * (time_in_seconds / 3600);
}

void TodayMetricsProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile) {
  // Reduce granularity of the enabled_date field to nearest hour.
  system_profile->set_uma_enabled_date(RoundSecondsToHour(enabled_date_));

  // Reduce granularity of the install_date field to nearest hour.
  system_profile->set_install_date(RoundSecondsToHour(install_date_));
}

}  // namespace

TodayMetricsLogger* TodayMetricsLogger::GetInstance() {
  // |logger| is a singleton that should live as long as the application.
  // We do not delete it and it will be deleted when the application dies.
  static TodayMetricsLogger* logger = new TodayMetricsLogger();
  return logger;
}

void TodayMetricsLogger::RecordUserAction(base::UserMetricsAction action) {
  if (!log_ && !CreateNewLog()) {
    return;
  }
  log_->RecordUserAction(action.str_);
  PersistLogs();
}

void TodayMetricsLogger::PersistLogs() {
  base::StatisticsRecorder::PrepareDeltas(false, base::Histogram::kNoFlags,
                                          base::Histogram::kNoFlags,
                                          &histogram_snapshot_manager_);
  std::string encoded_log;
  log_->GetOpenEncodedLog(&encoded_log);
  NSData* ns_encoded_log =
      [NSData dataWithBytes:encoded_log.c_str() length:encoded_log.length()];
  [[NSUserDefaults standardUserDefaults]
      setObject:ns_encoded_log
         forKey:kTodayExtensionMetricsCurrentLog];
  log_->TruncateEvents();
  if ((base::TimeTicks::Now() - log_->creation_time()).InSeconds() >=
      kMaxLogLifeTimeInSeconds) {
    CreateNewLog();
  }
}

bool TodayMetricsLogger::CreateNewLog() {
  id previous_log = [[NSUserDefaults standardUserDefaults]
      dataForKey:kTodayExtensionMetricsCurrentLog];
  if (previous_log) {
    app_group::client_app::AddPendingLog(previous_log,
                                         app_group::APP_GROUP_TODAY_EXTENSION);
    thread_pool_->PostTask(
        FROM_HERE, base::Bind(&app_group::client_app::CleanOldPendingLogs));
  }

  base::scoped_nsobject<NSUserDefaults> shared_defaults(
      [[NSUserDefaults alloc] initWithSuiteName:app_group::ApplicationGroup()]);

  NSString* client_id = [shared_defaults
      stringForKey:base::SysUTF8ToNSString(app_group::kChromeAppClientID)];
  NSString* enabled_date = [shared_defaults
      stringForKey:base::SysUTF8ToNSString(app_group::kUserMetricsEnabledDate)];
  NSString* install_date = [shared_defaults
      stringForKey:base::SysUTF8ToNSString(app_group::kInstallDate)];

  if (!client_id || !enabled_date || !install_date) {
    return false;
  }

  int session_id = [[NSUserDefaults standardUserDefaults]
      integerForKey:kTodayExtensionMetricsSessionID];
  [[NSUserDefaults standardUserDefaults]
      setInteger:session_id + 1
          forKey:kTodayExtensionMetricsSessionID];
  session_id = app_group::AppGroupSessionID(
      session_id, app_group::APP_GROUP_TODAY_EXTENSION);
  log_.reset(new TodayMetricsLog(base::SysNSStringToUTF8(client_id), session_id,
                                 metrics::MetricsLog::ONGOING_LOG,
                                 metrics_service_client_.get()));

  metrics::DelegatingProvider delegating_provider;
  delegating_provider.RegisterMetricsProvider(
      base::MakeUnique<TodayMetricsProvider>([enabled_date longLongValue],
                                             [install_date longLongValue]));
  log_->RecordEnvironment(&delegating_provider);
  return true;
}

TodayMetricsLogger::TodayMetricsLogger()
    : thread_pool_(
          new base::SequencedWorkerPool(2,
                                        "LoggerPool",
                                        base::TaskPriority::BACKGROUND)),
      metrics_service_client_(new TodayMetricsServiceClient()),
      histogram_snapshot_manager_(this) {
  base::StatisticsRecorder::Initialize();
}

TodayMetricsLogger::~TodayMetricsLogger() {}

void TodayMetricsLogger::RecordDelta(const base::HistogramBase& histogram,
                                     const base::HistogramSamples& snapshot) {
  log_->RecordHistogramDelta(histogram.histogram_name(), snapshot);
}

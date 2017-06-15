// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/config.h"

#include <string>

#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "components/download/public/features.h"

namespace download {

namespace {

// Default value for max concurrent downloads configuration.
const uint32_t kDefaultMaxConcurrentDownloads = 4;

// Default value for maximum running downloads of the download service.
const uint32_t kDefaultMaxRunningDownloads = 1;

// Default value for maximum scheduled downloads.
const uint32_t kDefaultMaxScheduledDownloads = 15;

// Default value for maximum retry count.
const uint32_t kDefaultMaxRetryCount = 5;

// Default value for file keep alive time in minutes, keep the file alive for
// 12 hours by default.
const uint32_t kDefaultFileKeepAliveTimeMinutes = 12 * 60;

// Default value for the start window time for OS to schedule background task.
const uint32_t kDefaultWindowStartTimeSeconds = 300; /* 5 minutes. */

// Default value for the end window time for OS to schedule background task.
const uint32_t kDefaultWindowEndTimeSeconds = 3600 * 8; /* 8 hours. */

// Helper routine to get Finch experiment parameter. If no Finch seed was found,
// use the |default_value|. The |name| should match an experiment
// parameter in Finch server configuration.
uint32_t GetFinchConfigUInt(const std::string& name, uint32_t default_value) {
  std::string finch_value =
      base::GetFieldTrialParamValueByFeature(kDownloadServiceFeature, name);
  uint32_t result;
  return base::StringToUint(finch_value, &result) ? result : default_value;
}

}  // namespace

// static
std::unique_ptr<Configuration> Configuration::CreateFromFinch() {
  std::unique_ptr<Configuration> config(new Configuration());
  config->max_concurrent_downloads = GetFinchConfigUInt(
      kMaxConcurrentDownloadsConfig, kDefaultMaxConcurrentDownloads);
  config->max_running_downloads = GetFinchConfigUInt(
      kMaxRunningDownloadsConfig, kDefaultMaxRunningDownloads);
  config->max_scheduled_downloads = GetFinchConfigUInt(
      kMaxScheduledDownloadsConfig, kDefaultMaxScheduledDownloads);
  config->max_retry_count =
      GetFinchConfigUInt(kMaxRetryCountConfig, kDefaultMaxRetryCount);
  config->file_keep_alive_time =
      base::TimeDelta::FromMinutes(base::saturated_cast<int>(GetFinchConfigUInt(
          kFileKeepAliveTimeMinutesConfig, kDefaultFileKeepAliveTimeMinutes)));
  config->window_start_time_seconds = GetFinchConfigUInt(
      kWindowStartTimeConfig, kDefaultWindowStartTimeSeconds);
  config->window_end_time_seconds =
      GetFinchConfigUInt(kWindowEndTimeConfig, kDefaultWindowEndTimeSeconds);
  return config;
}

Configuration::Configuration()
    : max_concurrent_downloads(kDefaultMaxConcurrentDownloads),
      max_running_downloads(kDefaultMaxRunningDownloads),
      max_scheduled_downloads(kDefaultMaxScheduledDownloads),
      max_retry_count(kDefaultMaxRetryCount),
      file_keep_alive_time(base::TimeDelta::FromMinutes(
          base::saturated_cast<int>(kDefaultFileKeepAliveTimeMinutes))),
      window_start_time_seconds(kDefaultWindowStartTimeSeconds),
      window_end_time_seconds(kDefaultWindowEndTimeSeconds) {}

}  // namespace download

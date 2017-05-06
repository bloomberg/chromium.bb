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
const int kDefaultMaxConcurrentDownloads = 4;

// Default value for maximum running downloads of the download service.
const int kDefaultMaxRunningDownloads = 1;

// Default value for maximum scheduled downloads.
const int kDefaultMaxScheduledDownloads = 15;

// Default value for maximum retry count.
const int kDefaultMaxRetryCount = 5;

// Default value for file keep alive time in minutes, keep the file alive for
// 12 hours by default.
const int kDefaultFileKeepAliveTimeMinutes = 12 * 60;

// Helper routine to get Finch experiment parameter. If no Finch seed was found,
// use the |default_value|. The |name| should match an experiment
// parameter in Finch server configuration.
int GetFinchConfigInt(const std::string& name, int default_value) {
  std::string finch_value =
      base::GetFieldTrialParamValueByFeature(kDownloadServiceFeature, name);
  int result;
  return base::StringToInt(finch_value, &result) ? result : default_value;
}

}  // namespace

// static
std::unique_ptr<Configuration> Configuration::CreateFromFinch() {
  std::unique_ptr<Configuration> config(new Configuration());
  config->max_concurrent_downloads = GetFinchConfigInt(
      kMaxConcurrentDownloadsConfig, kDefaultMaxConcurrentDownloads);
  config->max_running_downloads = GetFinchConfigInt(
      kMaxRunningDownloadsConfig, kDefaultMaxRunningDownloads);
  config->max_scheduled_downloads = GetFinchConfigInt(
      kMaxScheduledDownloadsConfig, kDefaultMaxScheduledDownloads);
  config->max_retry_count =
      GetFinchConfigInt(kMaxRetryCountConfig, kDefaultMaxRetryCount);
  config->file_keep_alive_time = base::TimeDelta::FromMinutes(GetFinchConfigInt(
      kFileKeepAliveTimeMinutesConfig, kDefaultFileKeepAliveTimeMinutes));
  return config;
}

Configuration::Configuration()
    : max_concurrent_downloads(kDefaultMaxConcurrentDownloads),
      max_running_downloads(kDefaultMaxRunningDownloads),
      max_scheduled_downloads(kDefaultMaxScheduledDownloads),
      max_retry_count(kDefaultMaxRetryCount),
      file_keep_alive_time(
          base::TimeDelta::FromMinutes(kDefaultFileKeepAliveTimeMinutes)) {}

}  // namespace download

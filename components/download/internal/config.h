// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_CONFIG_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_CONFIG_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"

namespace download {

// Configuration name for max concurrent downloads.
constexpr char kMaxConcurrentDownloadsConfig[] = "max_concurrent_downloads";

// Configuration name for maximum running downloads.
constexpr char kMaxRunningDownloadsConfig[] = "max_running_downloads";

// Configuration name for maximum scheduled downloads.
constexpr char kMaxScheduledDownloadsConfig[] = "max_scheduled_downloads";

// Configuration name for maximum retry count.
constexpr char kMaxRetryCountConfig[] = "max_retry_count";

// Configuration name for file keep alive time.
constexpr char kFileKeepAliveTimeMinutesConfig[] = "file_keep_alive_time";

// Download service configuration.
//
// Loaded based on experiment parameters from the server. Use default values if
// no server configuration was detected.
struct Configuration {
 public:
  // Create the configuration.
  static std::unique_ptr<Configuration> CreateFromFinch();
  Configuration();

  // The maximum number of downloads the DownloadService can have currently in
  // Active or Paused states.
  int max_concurrent_downloads;

  // The maximum number of downloads the DownloadService can have currently in
  // only Active state.
  int max_running_downloads;

  // The maximum number of downloads that are scheduled but not yet in Active
  // state, for each client using the download service.
  int max_scheduled_downloads;

  // The maximum number of retries before the download is aborted.
  int max_retry_count;

  // The time that the download service will keep the files around before
  // deleting them if the client hasn't handle the files.
  base::TimeDelta file_keep_alive_time;

 private:
  DISALLOW_COPY_AND_ASSIGN(Configuration);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_CONFIG_H_

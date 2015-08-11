// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/common/app_group/app_group_metrics_mainapp.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#include "ios/chrome/common/app_group/app_group_metrics.h"

namespace app_group {

namespace main_app {

void ProcessPendingLogs(
    const base::mac::ScopedBlock<ProceduralBlockWithData>& callback) {
  NSFileManager* file_manager = [NSFileManager defaultManager];
  NSURL* store_url = [file_manager
      containerURLForSecurityApplicationGroupIdentifier:ApplicationGroup()];
  NSURL* log_dir_url = [store_url
      URLByAppendingPathComponent:app_group::kPendingLogFileDirectory];

  NSArray* pending_logs =
      [file_manager contentsOfDirectoryAtPath:[log_dir_url path] error:nil];
  if (!pending_logs)
    return;
  for (NSString* pending_log : pending_logs) {
    if ([pending_log hasSuffix:app_group::kPendingLogFileSuffix]) {
      NSURL* file_url = [log_dir_url URLByAppendingPathComponent:pending_log];
      if (callback) {
        NSData* log_content = [file_manager contentsAtPath:[file_url path]];
        callback.get()(log_content);
      }
      [file_manager removeItemAtURL:file_url error:nil];
    }
  }
}

void EnableMetrics(NSString* client_id,
                   NSString* brand_code,
                   int64 install_date,
                   int64 enable_metrics_date) {
  base::scoped_nsobject<NSUserDefaults> shared_defaults(
      [[NSUserDefaults alloc] initWithSuiteName:ApplicationGroup()]);
  [shared_defaults setObject:client_id forKey:@(kChromeAppClientID)];

  [shared_defaults
      setObject:[NSString stringWithFormat:@"%lld", enable_metrics_date]
         forKey:@(kUserMetricsEnabledDate)];

  [shared_defaults setObject:[NSString stringWithFormat:@"%lld", install_date]
                      forKey:@(kInstallDate)];

  [shared_defaults setObject:brand_code forKey:@(kBrandCode)];
}

void DisableMetrics() {
  base::scoped_nsobject<NSUserDefaults> shared_defaults(
      [[NSUserDefaults alloc] initWithSuiteName:ApplicationGroup()]);
  [shared_defaults removeObjectForKey:@(kChromeAppClientID)];
}

}  // namespace main_app

}  // namespace app_group

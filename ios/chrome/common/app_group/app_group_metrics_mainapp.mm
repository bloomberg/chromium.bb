// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/common/app_group/app_group_metrics_mainapp.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#include "ios/chrome/common/app_group/app_group_metrics.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace app_group {

namespace main_app {

void ProcessPendingLogs(ProceduralBlockWithData callback) {
  base::ThreadRestrictions::AssertIOAllowed();
  NSFileManager* file_manager = [NSFileManager defaultManager];
  NSURL* store_url = [file_manager
      containerURLForSecurityApplicationGroupIdentifier:ApplicationGroup()];
  NSURL* log_dir_url =
      [store_url URLByAppendingPathComponent:app_group::kPendingLogFileDirectory
                                 isDirectory:YES];

  NSArray* pending_logs =
      [file_manager contentsOfDirectoryAtPath:[log_dir_url path] error:nil];
  if (!pending_logs)
    return;
  for (NSString* pending_log : pending_logs) {
    if ([pending_log hasSuffix:app_group::kPendingLogFileSuffix]) {
      NSURL* file_url =
          [log_dir_url URLByAppendingPathComponent:pending_log isDirectory:NO];
      if (callback) {
        NSData* log_content = [file_manager contentsAtPath:[file_url path]];
        callback(log_content);
      }
      [file_manager removeItemAtURL:file_url error:nil];
    }
  }
}

void EnableMetrics(NSString* client_id,
                   NSString* brand_code,
                   int64_t install_date,
                   int64_t enable_metrics_date) {
  NSUserDefaults* shared_defaults =
      [[NSUserDefaults alloc] initWithSuiteName:ApplicationGroup()];
  [shared_defaults setObject:client_id forKey:@(kChromeAppClientID)];

  [shared_defaults
      setObject:[NSString stringWithFormat:@"%lld", enable_metrics_date]
         forKey:@(kUserMetricsEnabledDate)];

  [shared_defaults setObject:[NSString stringWithFormat:@"%lld", install_date]
                      forKey:@(kInstallDate)];

  [shared_defaults setObject:brand_code forKey:@(kBrandCode)];
}

void DisableMetrics() {
  NSUserDefaults* shared_defaults =
      [[NSUserDefaults alloc] initWithSuiteName:ApplicationGroup()];
  [shared_defaults removeObjectForKey:@(kChromeAppClientID)];
}

}  // namespace main_app

}  // namespace app_group

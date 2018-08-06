// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/breakpad_helper.h"

#import <Foundation/Foundation.h>
#include <stddef.h>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/debug/crash_logging.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "ios/chrome/browser/chrome_paths.h"
#import "ios/chrome/browser/crash_report/crash_report_user_application_state.h"

// TODO(stuartmorgan): Move this up where it belongs once
// https://crbug.com/google-breakpad/487 is fixed. For now, put it at the end to
// avoid compiler errors.
#import "third_party/breakpad/breakpad/src/client/ios/BreakpadController.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace breakpad_helper {

namespace {

// Key in NSUserDefaults for a Boolean value that stores whether to upload
// crash reports.
NSString* const kCrashReportsUploadingEnabledKey =
    @"CrashReportsUploadingEnabled";

NSString* const kCrashedInBackground = @"crashed_in_background";
NSString* const kFreeDiskInKB = @"free_disk_in_kb";
NSString* const kFreeMemoryInKB = @"free_memory_in_kb";
NSString* const kMemoryWarningInProgress = @"memory_warning_in_progress";
NSString* const kMemoryWarningCount = @"memory_warning_count";
NSString* const kUptimeAtRestoreInMs = @"uptime_at_restore_in_ms";
NSString* const kUploadedInRecoveryMode = @"uploaded_in_recovery_mode";

// Multiple state information are combined into one CrachReportMultiParameter
// to save limited and finite number of ReportParameters.
// These are the values grouped in the user_application_state parameter.
NSString* const kOrientationState = @"orient";
NSString* const kHorizontalSizeClass = @"sizeclass";
NSString* const kSignedIn = @"signIn";
NSString* const kIsShowingPDF = @"pdf";
NSString* const kVideoPlaying = @"avplay";

// Whether the crash reporter is enabled.
bool g_crash_reporter_enabled = false;

void DeleteAllReportsInDirectory(base::FilePath directory) {
  base::FileEnumerator enumerator(directory, false,
                                  base::FileEnumerator::FILES);
  base::FilePath cur_file;
  while (!(cur_file = enumerator.Next()).value().empty()) {
    if (cur_file.BaseName().value() != kReporterLogFilename)
      base::DeleteFile(cur_file, false);
  }
}

// Callback for logging::SetLogMessageHandler
bool FatalMessageHandler(int severity,
                         const char* file,
                         int line,
                         size_t message_start,
                         const std::string& str) {
  // Do not handle non-FATAL.
  if (severity != logging::LOG_FATAL)
    return false;

  // In case of OOM condition, this code could be reentered when
  // constructing and storing the key.  Using a static is not
  // thread-safe, but if multiple threads are in the process of a
  // fatal crash at the same time, this should work.
  static bool guarded = false;
  if (guarded)
    return false;

  base::AutoReset<bool> guard(&guarded, true);

  // Only log last path component.  This matches logging.cc.
  if (file) {
    const char* slash = strrchr(file, '/');
    if (slash)
      file = slash + 1;
  }

  NSString* fatal_key = @"LOG_FATAL";
  NSString* fatal_value = [NSString
      stringWithFormat:@"%s:%d: %s", file, line, str.c_str() + message_start];
  AddReportParameter(fatal_key, fatal_value, true);

  // Rather than including the code to force the crash here, allow the
  // caller to do it.
  return false;
}

// Caches the uploading flag in NSUserDefaults, so that we can access the value
// in safe mode.
void CacheUploadingEnabled(bool uploading_enabled) {
  NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
  [user_defaults setBool:uploading_enabled ? YES : NO
                  forKey:kCrashReportsUploadingEnabledKey];
}

}  // namespace

void Start(const std::string& channel_name) {
  DCHECK(!g_crash_reporter_enabled);
  [[BreakpadController sharedInstance] start:YES];
  logging::SetLogMessageHandler(&FatalMessageHandler);
  g_crash_reporter_enabled = true;
  // Register channel information.
  if (channel_name.length()) {
    AddReportParameter(@"channel", base::SysUTF8ToNSString(channel_name), true);
  }
  // Notifying the PathService on the location of the crashes so that crashes
  // can be displayed to the user on the about:crashes page.
  NSArray* cachesDirectories = NSSearchPathForDirectoriesInDomains(
      NSCachesDirectory, NSUserDomainMask, YES);
  NSString* cachePath = [cachesDirectories objectAtIndex:0];
  NSString* dumpDirectory =
      [cachePath stringByAppendingPathComponent:@kDefaultLibrarySubdirectory];
  base::PathService::Override(
      ios::DIR_CRASH_DUMPS,
      base::FilePath(base::SysNSStringToUTF8(dumpDirectory)));
}

void SetEnabled(bool enabled) {
  if (g_crash_reporter_enabled == enabled)
    return;
  g_crash_reporter_enabled = enabled;
  if (g_crash_reporter_enabled) {
    [[BreakpadController sharedInstance] start:NO];
  } else {
    [[BreakpadController sharedInstance] stop];
    CacheUploadingEnabled(false);
  }
}

void SetUploadingEnabled(bool enabled) {
  CacheUploadingEnabled(g_crash_reporter_enabled && enabled);

  if (!g_crash_reporter_enabled)
    return;
  [[BreakpadController sharedInstance] setUploadingEnabled:enabled];
}

bool IsUploadingEnabled() {
  // Return the value cached by CacheUploadingEnabled().
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:kCrashReportsUploadingEnabledKey];
}

void CleanupCrashReports() {
  base::FilePath crash_directory;
  base::PathService::Get(ios::DIR_CRASH_DUMPS, &crash_directory);
  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&DeleteAllReportsInDirectory, crash_directory));
}

void AddReportParameter(NSString* key, NSString* value, bool async) {
  if (!g_crash_reporter_enabled)
    return;
  if (async) {
    [[BreakpadController sharedInstance] addUploadParameter:value forKey:key];
    return;
  }
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  [[BreakpadController sharedInstance] withBreakpadRef:^(BreakpadRef ref) {
    if (ref)
      BreakpadAddUploadParameter(ref, key, value);
    dispatch_semaphore_signal(semaphore);
  }];
  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
}

int GetCrashReportCount() {
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block int outerCrashReportCount = 0;
  [[BreakpadController sharedInstance] getCrashReportCount:^(int count) {
    outerCrashReportCount = count;
    dispatch_semaphore_signal(semaphore);
  }];
  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
  return outerCrashReportCount;
}

void GetCrashReportCount(void (^callback)(int)) {
  [[BreakpadController sharedInstance] getCrashReportCount:callback];
}

bool HasReportToUpload() {
  return GetCrashReportCount() > 0;
}

void RemoveReportParameter(NSString* key) {
  if (!g_crash_reporter_enabled)
    return;
  [[BreakpadController sharedInstance] removeUploadParameterForKey:key];
}

void SetCurrentlyInBackground(bool background) {
  if (background)
    AddReportParameter(kCrashedInBackground, @"yes", true);
  else
    RemoveReportParameter(kCrashedInBackground);
}

void SetMemoryWarningCount(int count) {
  if (count) {
    AddReportParameter(kMemoryWarningCount,
                       [NSString stringWithFormat:@"%d", count], true);
  } else {
    RemoveReportParameter(kMemoryWarningCount);
  }
}

void SetMemoryWarningInProgress(bool value) {
  if (value)
    AddReportParameter(kMemoryWarningInProgress, @"yes", true);
  else
    RemoveReportParameter(kMemoryWarningInProgress);
}

void SetCurrentFreeMemoryInKB(int value) {
  AddReportParameter(kFreeMemoryInKB, [NSString stringWithFormat:@"%d", value],
                     true);
}

void SetCurrentFreeDiskInKB(int value) {
  AddReportParameter(kFreeDiskInKB, [NSString stringWithFormat:@"%d", value],
                     true);
}

void SetCurrentTabIsPDF(bool value) {
  if (value) {
    [[CrashReportUserApplicationState sharedInstance]
        incrementValue:kIsShowingPDF];
  } else {
    [[CrashReportUserApplicationState sharedInstance]
        decrementValue:kIsShowingPDF];
  }
}

void SetCurrentOrientation(int statusBarOrientation, int deviceOrientation) {
  DCHECK((statusBarOrientation < 10) && (deviceOrientation < 10));
  int deviceAndUIOrientation = 10 * statusBarOrientation + deviceOrientation;
  [[CrashReportUserApplicationState sharedInstance]
       setValue:kOrientationState
      withValue:deviceAndUIOrientation];
}

void SetCurrentHorizontalSizeClass(int horizontalSizeClass) {
  [[CrashReportUserApplicationState sharedInstance]
       setValue:kHorizontalSizeClass
      withValue:horizontalSizeClass];
}

void SetCurrentlySignedIn(bool signedIn) {
  if (signedIn) {
    [[CrashReportUserApplicationState sharedInstance] setValue:kSignedIn
                                                     withValue:1];
  } else {
    [[CrashReportUserApplicationState sharedInstance] removeValue:kSignedIn];
  }
}

void MediaStreamPlaybackDidStart() {
  [[CrashReportUserApplicationState sharedInstance]
      incrementValue:kVideoPlaying];
}

void MediaStreamPlaybackDidStop() {
  [[CrashReportUserApplicationState sharedInstance]
      decrementValue:kVideoPlaying];
}

// Records the current process uptime in the "uptime_at_restore_in_ms". This
// will allow engineers to dremel crash logs to find crash whose delta between
// process uptime at crash and process uptime at restore is smaller than X
// seconds and find insta-crashers.
void WillStartCrashRestoration() {
  if (!g_crash_reporter_enabled)
    return;
  // We use gettimeofday and BREAKPAD_PROCESS_START_TIME to compute the
  // uptime to stay as close as possible as how breakpad computes the
  // "ProcessUptime" in order to have meaningful comparison in dremel.
  struct timeval tv;
  gettimeofday(&tv, NULL);
  // The values stored in the breakpad log are only accessible through a
  // BreakpadRef. To record the process uptime at restore, the value of
  // BREAKPAD_PROCESS_START_TIME is required to compute the delta.
  [[BreakpadController sharedInstance] withBreakpadRef:^(BreakpadRef ref) {
    if (!ref)
      return;
    NSString* processStartTimeSecondsString =
        BreakpadKeyValue(ref, @BREAKPAD_PROCESS_START_TIME);
    if (!processStartTimeSecondsString)
      return;

    time_t processStartTimeSeconds =
        [processStartTimeSecondsString longLongValue];
    time_t processUptimeSeconds = tv.tv_sec - processStartTimeSeconds;
    unsigned long long processUptimeMilliseconds =
        static_cast<unsigned long long>(processUptimeSeconds) *
        base::Time::kMillisecondsPerSecond;
    BreakpadAddUploadParameter(
        ref, kUptimeAtRestoreInMs,
        [NSString stringWithFormat:@"%llu", processUptimeMilliseconds]);
  }];
}

void StartUploadingReportsInRecoveryMode() {
  if (!g_crash_reporter_enabled)
    return;
  [[BreakpadController sharedInstance] stop];
  [[BreakpadController sharedInstance] setParametersToAddAtUploadTime:@{
    kUploadedInRecoveryMode : @"yes"
  }];
  [[BreakpadController sharedInstance] setUploadInterval:1];
  [[BreakpadController sharedInstance] start:NO];
  [[BreakpadController sharedInstance] setUploadingEnabled:YES];
}

void RestoreDefaultConfiguration() {
  if (!g_crash_reporter_enabled)
    return;
  [[BreakpadController sharedInstance] stop];
  [[BreakpadController sharedInstance] resetConfiguration];
  [[BreakpadController sharedInstance] start:NO];
  [[BreakpadController sharedInstance] setUploadingEnabled:NO];
}

}  // namespace breakpad_helper

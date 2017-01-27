// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/crash_report_background_uploader.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#include "base/mac/scoped_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/time/time.h"
#import "breakpad/src/client/ios/BreakpadController.h"
#include "ios/chrome/browser/experimental_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

namespace {

NSString* const kBackgroundReportUploader =
    @"com.google.chrome.breakpad.backgroundupload";
const char* const kUMAMobileCrashBackgroundUploadDelay =
    "CrashReport.CrashBackgroundUploadDelay";
const char* const kUMAMobilePendingReportsOnBackgroundWakeUp =
    "CrashReport.PendingReportsOnBackgroundWakeUp";
NSString* const kUploadedInBackground = @"uploaded_in_background";
NSString* const kReportsUploadedInBackground = @"ReportsUploadedInBackground";

NSString* CreateSessionIdentifierFromTask(NSURLSessionTask* task) {
  return [NSString stringWithFormat:@"%@.%ld", kBackgroundReportUploader,
                                    (unsigned long)[task taskIdentifier]];
}

}  // namespace

@interface UrlSessionDelegate : NSObject<NSURLSessionDelegate,
                                         NSURLSessionTaskDelegate,
                                         NSURLSessionDataDelegate>
+ (instancetype)sharedInstance;

// Sets the completion handler for the URL session current tasks. The
// |completionHandler| cannot be nil.
- (void)setSessionCompletionHandler:(ProceduralBlock)completionHandler;

@end

@implementation UrlSessionDelegate {
  // The completion handler to call when all tasks are completed.
  base::mac::ScopedBlock<ProceduralBlock> _sessionCompletionHandler;
  // The number of tasks in progress for the session.
  int _tasks;
  // Flag to indicate that URLSessionDidFinishEventsForBackgroundURLSession
  // has been called, so that no new task will be launched for this session.
  // It is safe to call completion handler when the pending tasks are completed.
  BOOL _didFinishEventsCalled;
}

+ (instancetype)sharedInstance {
  static UrlSessionDelegate* instance = [[UrlSessionDelegate alloc] init];
  return instance;
}

- (void)setSessionCompletionHandler:(ProceduralBlock)completionHandler {
  DCHECK(completionHandler);
  _sessionCompletionHandler.reset(completionHandler);
  _didFinishEventsCalled = NO;
}

- (void)URLSession:(NSURLSession*)session
                   task:(NSURLSessionTask*)dataTask
    didReceiveChallenge:(NSURLAuthenticationChallenge*)challenge
      completionHandler:
          (void (^)(NSURLSessionAuthChallengeDisposition disposition,
                    NSURLCredential* credential))completionHandler {
  if (![challenge.protectionSpace.authenticationMethod
          isEqualToString:NSURLAuthenticationMethodServerTrust]) {
    completionHandler(NSURLSessionAuthChallengeUseCredential, nil);
    return;
  }
  NSString* identifier = CreateSessionIdentifierFromTask(dataTask);

  NSDictionary* configuration =
      [[NSUserDefaults standardUserDefaults] dictionaryForKey:identifier];
  NSString* host =
      [[NSURL URLWithString:[configuration objectForKey:@BREAKPAD_URL]] host];
  if ([challenge.protectionSpace.host isEqualToString:host]) {
    NSURLCredential* credential = [NSURLCredential
        credentialForTrust:challenge.protectionSpace.serverTrust];
    completionHandler(NSURLSessionAuthChallengeUseCredential, credential);
    return;
  }
  completionHandler(NSURLSessionAuthChallengeUseCredential, nil);
}

- (void)URLSessionDidFinishEventsForBackgroundURLSession:
        (NSURLSession*)session {
  _didFinishEventsCalled = YES;
  [[NSOperationQueue mainQueue] addOperationWithBlock:^{
    [self callCompletionHandler];
  }];
}

- (void)taskFinished {
  DCHECK_GT(_tasks, 0);
  _tasks--;
  [[NSOperationQueue mainQueue] addOperationWithBlock:^{
    [self callCompletionHandler];
  }];
}

- (void)callCompletionHandler {
  if (_tasks > 0 || !_didFinishEventsCalled)
    return;
  if (_sessionCompletionHandler) {
    void (^completionHandler)() = _sessionCompletionHandler.get();
    completionHandler();
    _sessionCompletionHandler.reset();
  }
}

- (void)URLSession:(NSURLSession*)session
              dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveResponse:(NSURLResponse*)response
     completionHandler:
         (void (^)(NSURLSessionResponseDisposition disposition))handler {
  handler(NSURLSessionResponseAllow);
}

- (void)URLSession:(NSURLSession*)session
          dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveData:(NSData*)data {
  NSString* identifier = CreateSessionIdentifierFromTask(dataTask);

  NSDictionary* configuration =
      [[NSUserDefaults standardUserDefaults] dictionaryForKey:identifier];
  [[NSUserDefaults standardUserDefaults] removeObjectForKey:identifier];
  _tasks++;

  if (experimental_flags::IsAlertOnBackgroundUploadEnabled()) {
    base::scoped_nsobject<UILocalNotification> localNotification(
        [[UILocalNotification alloc] init]);
    localNotification.get().fireDate = [NSDate date];
    base::scoped_nsobject<NSString> reportId(
        [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]);
    localNotification.get().alertBody = [NSString
        stringWithFormat:@"Crash report uploaded: %@", reportId.get()];
    [[UIApplication sharedApplication]
        scheduleLocalNotification:localNotification];
  }

  [[BreakpadController sharedInstance] withBreakpadRef:^(BreakpadRef ref) {
    BreakpadHandleNetworkResponse(ref, configuration, data, nil);
    dispatch_async(dispatch_get_main_queue(), ^{
      [self taskFinished];
    });
  }];
}

@end

@implementation CrashReportBackgroundUploader

@synthesize hasPendingCrashReportsToUploadAtStartup;

+ (instancetype)sharedInstance {
  static CrashReportBackgroundUploader* instance =
      [[CrashReportBackgroundUploader alloc] init];
  return instance;
}

+ (NSURLSession*)BreakpadBackgroundURLSessionWithCompletionHandler:
        (ProceduralBlock)completionHandler {
  static NSURLSession* session = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    NSURLSessionConfiguration* sessionConfig = [NSURLSessionConfiguration
        backgroundSessionConfigurationWithIdentifier:kBackgroundReportUploader];
    session = [NSURLSession
        sessionWithConfiguration:sessionConfig
                        delegate:[UrlSessionDelegate sharedInstance]
                   delegateQueue:[NSOperationQueue mainQueue]];
  });
  DCHECK(session);
  if (completionHandler) {
    [[UrlSessionDelegate sharedInstance]
        setSessionCompletionHandler:completionHandler];
  }
  return session;
}

+ (BOOL)sendNextReport:(NSDictionary*)nextReport
       withBreakpadRef:(BreakpadRef)ref {
  NSString* uploadURL =
      [NSString stringWithString:[nextReport valueForKey:@BREAKPAD_URL]];
  NSString* tmpDir = NSTemporaryDirectory();
  NSString* tmpFile = [tmpDir
      stringByAppendingPathComponent:
          [NSString
              stringWithFormat:@"%.0f.%@",
                               [NSDate timeIntervalSinceReferenceDate] * 1000.0,
                               @"txt"]];
  NSURL* fileURL = [NSURL fileURLWithPath:tmpFile];
  [nextReport setValue:[fileURL absoluteString] forKey:@BREAKPAD_URL];

#ifndef NDEBUG
  NSString* BreakpadMinidumpLocation = [NSHomeDirectory()
      stringByAppendingPathComponent:@"Library/Caches/Breakpad"];
  [nextReport setValue:BreakpadMinidumpLocation
                forKey:@kReporterMinidumpDirectoryKey];
  [nextReport setValue:BreakpadMinidumpLocation
                forKey:@BREAKPAD_DUMP_DIRECTORY];
#endif

  [[BreakpadController sharedInstance]
      threadUnsafeSendReportWithConfiguration:nextReport
                              withBreakpadRef:ref];

  NSFileManager* fileManager = [NSFileManager defaultManager];
  if (![fileManager fileExistsAtPath:tmpFile]) {
    return NO;
  }

  NSError* error;
  NSString* fileString =
      [NSString stringWithContentsOfFile:tmpFile
                                encoding:NSISOLatin1StringEncoding
                                   error:&error];

  // The HTTP content is a MIME multipart. The delimiter of the mime body must
  // be added to the HTTP headers.
  // A mime body is of the form
  // --{delimiter}
  // content 1
  // --{delimiter}
  // content 2
  // --{delimiter}--
  // The delimiter can be read on the first line of the file.
  NSString* delimiter =
      [[fileString componentsSeparatedByCharactersInSet:
                       [NSCharacterSet newlineCharacterSet]] firstObject];
  if (![delimiter hasPrefix:@"--"]) {
    [fileManager removeItemAtPath:tmpFile error:&error];
    return NO;
  }
  delimiter = [[delimiter
      stringByTrimmingCharactersInSet:[NSCharacterSet newlineCharacterSet]]
      substringFromIndex:2];

  NSMutableURLRequest* request =
      [NSMutableURLRequest requestWithURL:[NSURL URLWithString:uploadURL]];
  [request setHTTPMethod:@"POST"];
  [request setValue:[NSString
                        stringWithFormat:@"multipart/form-data; boundary=%@",
                                         delimiter]
      forHTTPHeaderField:@"Content-type"];
  [request setHTTPBody:[NSData dataWithContentsOfFile:tmpFile]];

  NSURLSession* session = [CrashReportBackgroundUploader
      BreakpadBackgroundURLSessionWithCompletionHandler:nil];
  NSURLSessionDataTask* dataTask =
      [session uploadTaskWithRequest:request fromFile:fileURL];

  NSString* identifier = CreateSessionIdentifierFromTask(dataTask);
  [[NSUserDefaults standardUserDefaults] setObject:nextReport
                                            forKey:identifier];

  [dataTask resume];
  return YES;
}

+ (void)performFetchWithCompletionHandler:
        (BackgroundFetchCompletionBlock)completionHandler {
  [[BreakpadController sharedInstance] stop];
  [[BreakpadController sharedInstance] setParametersToAddAtUploadTime:@{
    kUploadedInBackground : @"yes"
  }];
  [[BreakpadController sharedInstance] start:YES];
  [[BreakpadController sharedInstance] withBreakpadRef:^(BreakpadRef ref) {
    // Note that this processing will be done before |sendNextCrashReport|
    // starts uploading the crashes. The ordering is ensured here because both
    // the crash report processing and the upload enabling are handled by
    // posting blocks to a single |dispath_queue_t| in BreakpadController.
    [[BreakpadController sharedInstance] setUploadingEnabled:YES];
    [[BreakpadController sharedInstance]
        getNextReportConfigurationOrSendDelay:^(NSDictionary* nextReport,
                                                int delay) {
          BOOL reportToSend = NO;
          BOOL uploaded = NO;
          UMA_HISTOGRAM_COUNTS_100(kUMAMobilePendingReportsOnBackgroundWakeUp,
                                   BreakpadGetCrashReportCount(ref));
          if (delay == 0 && nextReport) {
            reportToSend = YES;
            NSNumber* crashTimeNum =
                [nextReport valueForKey:@BREAKPAD_PROCESS_CRASH_TIME];
            base::Time crashTime =
                base::Time::FromTimeT([crashTimeNum intValue]);
            base::Time now = base::Time::Now();
            UMA_HISTOGRAM_LONG_TIMES_100(kUMAMobileCrashBackgroundUploadDelay,
                                         now - crashTime);
            uploaded = [self sendNextReport:nextReport withBreakpadRef:ref];
          }
          int pendingReports = BreakpadGetCrashReportCount(ref);
          [[BreakpadController sharedInstance] setUploadingEnabled:NO];
          dispatch_async(dispatch_get_main_queue(), ^{
            if (reportToSend) {
              if (uploaded) {
                NSUserDefaults* defaults =
                    [NSUserDefaults standardUserDefaults];
                NSInteger uploadedCrashes =
                    [defaults integerForKey:kReportsUploadedInBackground];
                [defaults setInteger:(uploadedCrashes + 1)
                              forKey:kReportsUploadedInBackground];
                base::RecordAction(
                    UserMetricsAction("BackgroundUploadReportSucceeded"));

              } else {
                base::RecordAction(
                    UserMetricsAction("BackgroundUploadReportAborted"));
              }
            }
            if (uploaded && pendingReports) {
              completionHandler(UIBackgroundFetchResultNewData);
            } else if (pendingReports) {
              completionHandler(UIBackgroundFetchResultFailed);
            } else {
              [[UIApplication sharedApplication]
                  setMinimumBackgroundFetchInterval:
                      UIApplicationBackgroundFetchIntervalNever];
              completionHandler(UIBackgroundFetchResultNoData);
            }
          });
        }];
  }];
}

+ (BOOL)canHandleBackgroundURLSession:(NSString*)identifier {
  return [identifier isEqualToString:kBackgroundReportUploader];
}

+ (void)handleEventsForBackgroundURLSession:(NSString*)identifier
                          completionHandler:(ProceduralBlock)completionHandler {
  [CrashReportBackgroundUploader
      BreakpadBackgroundURLSessionWithCompletionHandler:completionHandler];
}

+ (BOOL)hasUploadedCrashReportsInBackground {
  NSInteger uploadedCrashReportsInBackgroundCount =
      [[NSUserDefaults standardUserDefaults]
          integerForKey:kReportsUploadedInBackground];
  return uploadedCrashReportsInBackgroundCount > 0;
}

+ (void)resetReportsUploadedInBackgroundCount {
  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:kReportsUploadedInBackground];
}

@end

// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_service_ios.h"

#import <UIKit/UIKit.h>

#include "base/critical_closure.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#import "base/mac/bind_objc_block.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_restrictions.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/web/public/crw_navigation_item_storage.h"
#import "ios/web/public/crw_session_certificate_policy_cache_storage.h"
#import "ios/web/public/crw_session_storage.h"
#include "ios/web/public/web_thread.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// When C++ exceptions are disabled, the C++ library defines |try| and
// |catch| so as to allow exception-expecting C++ code to build properly when
// language support for exceptions is not present.  These macros interfere
// with the use of |@try| and |@catch| in Objective-C files such as this one.
// Undefine these macros here, after everything has been #included, since
// there will be no C++ uses and only Objective-C uses from this point on.
#undef try
#undef catch

namespace {
const NSTimeInterval kSaveDelay = 2.5;     // Value taken from Desktop Chrome.
NSString* const kRootObjectKey = @"root";  // Key for the root object.
}

@implementation NSKeyedUnarchiver (CrLegacySessionCompatibility)

// When adding a new compatibility alias here, create a new crbug to track its
// removal and mark it with a release at least one year after the introduction
// of the alias.
- (void)cr_registerCompatibilityAliases {
  // TODO(crbug.com/661633): those aliases where introduced between M57 and
  // M58, so remove them after M67 has shipped to stable.
  [self setClass:[CRWSessionCertificatePolicyCacheStorage class]
      forClassName:@"SessionCertificatePolicyManager"];
  [self setClass:[CRWSessionStorage class] forClassName:@"SessionController"];
  [self setClass:[CRWSessionStorage class]
      forClassName:@"CRWSessionController"];
  [self setClass:[CRWNavigationItemStorage class] forClassName:@"SessionEntry"];
  [self setClass:[CRWNavigationItemStorage class]
      forClassName:@"CRWSessionEntry"];
  [self setClass:[SessionWindowIOS class] forClassName:@"SessionWindow"];

  // TODO(crbug.com/661633): this alias was introduced between M58 and M59, so
  // remove it after M68 has shipped to stable.
  [self setClass:[CRWSessionStorage class]
      forClassName:@"CRWNavigationManagerStorage"];
  [self setClass:[CRWSessionCertificatePolicyCacheStorage class]
      forClassName:@"CRWSessionCertificatePolicyManager"];
}

@end

@implementation SessionServiceIOS {
  // The SequencedTaskRunner on which File IO operations are performed.
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;

  // Maps session path to the pending session window for the delayed save
  // behaviour.
  NSMutableDictionary<NSString*, SessionWindowIOS*>* _pendingSessionWindows;
}

#pragma mark - NSObject overrides

- (instancetype)init {
  base::SequencedWorkerPool* pool = web::WebThread::GetBlockingPool();
  scoped_refptr<base::SequencedTaskRunner> taskRunner =
      pool->GetSequencedTaskRunner(pool->GetSequenceToken());
  return [self initWithTaskRunner:taskRunner];
}

#pragma mark - Public interface

+ (SessionServiceIOS*)sharedService {
  static SessionServiceIOS* singleton = nil;
  if (!singleton) {
    singleton = [[[self class] alloc] init];
  }
  return singleton;
}

- (instancetype)initWithTaskRunner:
    (const scoped_refptr<base::SequencedTaskRunner>&)taskRunner {
  DCHECK(taskRunner);
  self = [super init];
  if (self) {
    _pendingSessionWindows = [NSMutableDictionary dictionary];
    _taskRunner = taskRunner;
  }
  return self;
}

- (void)saveSessionWindow:(SessionWindowIOS*)sessionWindow
                directory:(NSString*)directory
              immediately:(BOOL)immediately {
  NSString* sessionPath = [[self class] sessionPathForDirectory:directory];
  BOOL hadPendingSession =
      [_pendingSessionWindows objectForKey:sessionPath] != nil;
  [_pendingSessionWindows setObject:sessionWindow forKey:sessionPath];
  if (immediately) {
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    [self performSaveToPathInBackground:sessionPath];
  } else if (!hadPendingSession) {
    // If there wasn't previously a delayed save pending for |sessionPath|,
    // enqueue one now.
    [self performSelector:@selector(performSaveToPathInBackground:)
               withObject:sessionPath
               afterDelay:kSaveDelay];
  }
}

- (SessionWindowIOS*)loadSessionWindowFromDirectory:(NSString*)directory {
  NSString* sessionPath = [[self class] sessionPathForDirectory:directory];
  return [self loadSessionWindowFromPath:sessionPath];
}

- (SessionWindowIOS*)loadSessionWindowFromPath:(NSString*)sessionPath {
  @try {
    NSData* data = [NSData dataWithContentsOfFile:sessionPath];
    if (!data)
      return nil;

    NSKeyedUnarchiver* unarchiver =
        [[NSKeyedUnarchiver alloc] initForReadingWithData:data];

    // Register compatibility aliases to support legacy saved sessions.
    [unarchiver cr_registerCompatibilityAliases];
    return [unarchiver decodeObjectForKey:kRootObjectKey];
  } @catch (NSException* exception) {
    NOTREACHED() << "Error loading session file: "
                 << base::SysNSStringToUTF8(sessionPath) << ": "
                 << base::SysNSStringToUTF8([exception reason]);
    return nil;
  }
}

- (void)deleteLastSessionFileInDirectory:(NSString*)directory {
  NSString* sessionPath = [[self class] sessionPathForDirectory:directory];
  _taskRunner->PostTask(
      FROM_HERE, base::BindBlockArc(^{
        base::ThreadRestrictions::AssertIOAllowed();
        NSFileManager* fileManager = [NSFileManager defaultManager];
        if (![fileManager fileExistsAtPath:sessionPath])
          return;

        NSError* error = nil;
        if (![fileManager removeItemAtPath:sessionPath error:nil])
          CHECK(false) << "Unable to delete session file: "
                       << base::SysNSStringToUTF8(sessionPath) << ": "
                       << base::SysNSStringToUTF8([error description]);
      }));
}

+ (NSString*)sessionPathForDirectory:(NSString*)directory {
  return [directory stringByAppendingPathComponent:@"session.plist"];
}

#pragma mark - Private methods

// Do the work of saving on a background thread.
- (void)performSaveToPathInBackground:(NSString*)sessionPath {
  DCHECK(sessionPath);
  DCHECK([_pendingSessionWindows objectForKey:sessionPath] != nil);

  // Serialize to NSData on the main thread to avoid accessing potentially
  // non-threadsafe objects on a background thread.
  SessionWindowIOS* sessionWindow =
      [_pendingSessionWindows objectForKey:sessionPath];
  [_pendingSessionWindows removeObjectForKey:sessionPath];

  @try {
    NSData* sessionData =
        [NSKeyedArchiver archivedDataWithRootObject:sessionWindow];
    _taskRunner->PostTask(
        FROM_HERE, base::MakeCriticalClosure(base::BindBlockArc(^{
          [self performSaveSessionData:sessionData sessionPath:sessionPath];
        })));
  } @catch (NSException* exception) {
    NOTREACHED() << "Error serializing session for path: "
                 << base::SysNSStringToUTF8(sessionPath) << ": "
                 << base::SysNSStringToUTF8([exception description]);
    return;
  }
}

@end

@implementation SessionServiceIOS (SubClassing)

- (void)performSaveSessionData:(NSData*)sessionData
                   sessionPath:(NSString*)sessionPath {
  base::ThreadRestrictions::AssertIOAllowed();

  NSFileManager* fileManager = [NSFileManager defaultManager];
  NSString* directory = [sessionPath stringByDeletingLastPathComponent];

  NSError* error = nil;
  BOOL isDirectory = NO;
  if (![fileManager fileExistsAtPath:directory isDirectory:&isDirectory]) {
    isDirectory = YES;
    if (![fileManager createDirectoryAtPath:directory
                withIntermediateDirectories:YES
                                 attributes:nil
                                      error:&error]) {
      NOTREACHED() << "Error creating destination directory: "
                   << base::SysNSStringToUTF8(directory) << ": "
                   << base::SysNSStringToUTF8([error description]);
      return;
    }
  }

  if (!isDirectory) {
    NOTREACHED() << "Error creating destination directory: "
                 << base::SysNSStringToUTF8(directory) << ": "
                 << "file exists and is not a directory.";
    return;
  }

  NSDataWritingOptions options =
      NSDataWritingAtomic | NSDataWritingFileProtectionComplete;

  if (![sessionData writeToFile:sessionPath options:options error:&error]) {
    NOTREACHED() << "Error writing session file: "
                 << base::SysNSStringToUTF8(sessionPath) << ": "
                 << base::SysNSStringToUTF8([error description]);
    return;
  }
}

@end

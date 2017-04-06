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
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
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

@interface SessionServiceIOS () {
  // The SequencedTaskRunner on which File IO operations are performed.
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;

  // Maps save directories to the pending SessionWindow for the delayed
  // save behavior.
  NSMutableDictionary<NSString*, SessionWindowIOS*>* _pendingWindows;
}

// Saves the session corresponding to |directory| on the background
// task runner |_taskRunner|.
- (void)performSaveToDirectoryInBackground:(NSString*)directory;
@end

@implementation SessionServiceIOS

+ (SessionServiceIOS*)sharedService {
  static SessionServiceIOS* singleton = nil;
  if (!singleton) {
    singleton = [[[self class] alloc] init];
  }
  return singleton;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _pendingWindows = [NSMutableDictionary dictionary];
    base::SequencedWorkerPool* pool = web::WebThread::GetBlockingPool();
    _taskRunner = pool->GetSequencedTaskRunner(pool->GetSequenceToken());
  }
  return self;
}

// Returns the path of the session file.
- (NSString*)sessionFilePathForDirectory:(NSString*)directory {
  return [directory stringByAppendingPathComponent:@"session.plist"];
}

// Do the work of saving on a background thread. Assumes |window| is threadsafe.
- (void)performSaveToDirectoryInBackground:(NSString*)directory {
  DCHECK(directory);
  DCHECK([_pendingWindows objectForKey:directory] != nil);

  // Put the window into a local var so it can be retained for the block, yet
  // we can remove it from the dictionary to allow queuing another save.
  SessionWindowIOS* localWindow = [_pendingWindows objectForKey:directory];
  [_pendingWindows removeObjectForKey:directory];

  _taskRunner->PostTask(
      FROM_HERE, base::MakeCriticalClosure(base::BindBlockArc(^{
        @try {
          [self performSaveWindow:localWindow toDirectory:directory];
        } @catch (NSException* e) {
          // Do nothing.
        }
      })));
}

// Saves a SessionWindowIOS in a given directory. In case the directory doesn't
// exists it will be automatically created.
- (void)performSaveWindow:(SessionWindowIOS*)window
              toDirectory:(NSString*)directory {
  base::ThreadRestrictions::AssertIOAllowed();
  NSFileManager* fileManager = [NSFileManager defaultManager];
  BOOL isDir;
  if (![fileManager fileExistsAtPath:directory isDirectory:&isDir]) {
    NSError* error = nil;
    BOOL result = [fileManager createDirectoryAtPath:directory
                         withIntermediateDirectories:YES
                                          attributes:nil
                                               error:&error];
    DCHECK(result);
    if (!result) {
      DLOG(ERROR) << "Error creating destination directory: "
                  << base::SysNSStringToUTF8(directory) << ": "
                  << base::SysNSStringToUTF8([error description]);
      return;
    }
  } else {
    DCHECK(isDir);
    if (!isDir) {
      DLOG(ERROR) << "Error creating destination directory: "
                  << base::SysNSStringToUTF8(directory) << ": "
                  << "file exists and is not a directory.";
      return;
    }
  }

  NSString* filename = [self sessionFilePathForDirectory:directory];
  if (filename) {
    BOOL result = [NSKeyedArchiver archiveRootObject:window toFile:filename];
    DCHECK(result);
    if (!result) {
      DLOG(ERROR) << "Error writing session file to " << filename;
      return;
    }

    // Encrypt the session file (mostly for Incognito, but can't hurt to
    // always do it).
    NSError* error = nil;
    BOOL success = [[NSFileManager defaultManager]
        setAttributes:@{NSFileProtectionKey : NSFileProtectionComplete}
         ofItemAtPath:filename
                error:&error];
    if (!success) {
      DLOG(ERROR) << "Error encrypting session file: "
                  << base::SysNSStringToUTF8(filename) << ": "
                  << base::SysNSStringToUTF8([error description]);
    }
  }
}

- (void)saveWindow:(SessionWindowIOS*)window
    forBrowserState:(ios::ChromeBrowserState*)browserState
        immediately:(BOOL)immediately {
  NSString* stashPath =
      base::SysUTF8ToNSString(browserState->GetStatePath().value());
  BOOL hadPendingSession = [_pendingWindows objectForKey:stashPath] != nil;
  [_pendingWindows setObject:window forKey:stashPath];
  if (immediately) {
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    [self performSaveToDirectoryInBackground:stashPath];
  } else if (!hadPendingSession) {
    // If there wasn't previously a delayed save pending for |stashPath|,
    // enqueue one now.
    [self performSelector:@selector(performSaveToDirectoryInBackground:)
               withObject:stashPath
               afterDelay:kSaveDelay];
  }
}

- (SessionWindowIOS*)loadWindowForBrowserState:
    (ios::ChromeBrowserState*)browserState {
  NSString* stashPath =
      base::SysUTF8ToNSString(browserState->GetStatePath().value());
  SessionWindowIOS* window =
      [self loadWindowFromPath:[self sessionFilePathForDirectory:stashPath]];
  return window;
}

- (SessionWindowIOS*)loadWindowFromPath:(NSString*)sessionPath {
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
    DLOG(ERROR) << "Error loading session file: "
                << base::SysNSStringToUTF8(sessionPath) << ": "
                << base::SysNSStringToUTF8([exception reason]);
    return nil;
  }
}

// Deletes the file containing the commands for the last session in the given
// browserState directory.
- (void)deleteLastSession:(NSString*)directory {
  NSString* sessionPath = [self sessionFilePathForDirectory:directory];
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

@end

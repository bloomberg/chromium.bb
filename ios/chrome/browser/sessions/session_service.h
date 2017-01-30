// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_SERVICE_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_SERVICE_H_

#import <Foundation/Foundation.h>

#include "base/mac/scoped_nsobject.h"

namespace ios {
class ChromeBrowserState;
}

@class SessionWindowIOS;

// Trivial NSKeyedUnarchiver subclass that can be configured with a
// ChromeBrowserState instance that decoding classes can use.
@interface SessionWindowUnarchiver : NSKeyedUnarchiver

// The BrowserState set on the unarchiver at init; a weak pointer.
@property(nonatomic, readonly) ios::ChromeBrowserState* browserState;

// Inits exactly as initForReadingWithData: does, additionally setting
// |browserState| on the receiver.
- (instancetype)initForReadingWithData:(NSData*)data
                          browserState:(ios::ChromeBrowserState*)browserState;

@end

// A singleton service for saving the current session. Can either save on a
// delay or immediately. Saving is always performed on a separate thread.
@interface SessionServiceIOS : NSObject

// Lazily creates a singleton instance. Use this instead of calling alloc/init.
+ (SessionServiceIOS*)sharedService;

// Saves the session represented by |window| to the given browserState directory
// on disk. If |immediately| is NO, the save is done after a delay. If another
// call is pending, this one is ignored. If YES, the save is done now,
// cancelling any pending calls.  Either way, the save is done on a separate
// thread to avoid blocking the UI thread. As a result, |window| should contain
// copies of non-threadsafe objects.
- (void)saveWindow:(SessionWindowIOS*)window
    forBrowserState:(ios::ChromeBrowserState*)browserState
        immediately:(BOOL)immediately;

// Loads the window from the given browserState directory on disk on the main
// thread. Returns nil if no session was previously saved.
- (SessionWindowIOS*)loadWindowForBrowserState:
    (ios::ChromeBrowserState*)browserState;

// Schedules deletion of the file containing the commands for the last session
// in the given browserState directory.
- (void)deleteLastSession:(NSString*)directory;

// Loads the window from the given backup file on disk on the main thread.
// Returns nil if unable to read the sessions.
- (SessionWindowIOS*)loadWindowFromPath:(NSString*)path
                        forBrowserState:(ios::ChromeBrowserState*)browserState;

// Returns the path of the session file.
- (NSString*)sessionFilePathForDirectory:(NSString*)directory;

@end

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_SERVICE_H_

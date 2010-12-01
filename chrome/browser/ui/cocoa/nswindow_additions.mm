// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/nswindow_additions.h"

#include <dlfcn.h>

#include "base/logging.h"

typedef void* CGSConnectionID;
typedef int CGSWindowID;
typedef int CGSError;
typedef int CGSWorkspaceID;

// These are private APIs we look up at run time.
typedef CGSConnectionID (*CGSDefaultConnectionFunc)(void);
typedef CGSError (*CGSGetWindowWorkspaceFunc)(const CGSConnectionID cid,
                                              CGSWindowID wid,
                                              CGSWorkspaceID* workspace);
typedef CGSError (*CGSMoveWorkspaceWindowListFunc)(const CGSConnectionID cid,
                                                   CGSWindowID* wids,
                                                   int count,
                                                   CGSWorkspaceID workspace);

static CGSDefaultConnectionFunc sCGSDefaultConnection;
static CGSGetWindowWorkspaceFunc sCGSGetWindowWorkspace;
static CGSMoveWorkspaceWindowListFunc sCGSMoveWorkspaceWindowList;

@implementation NSWindow(ChromeAdditions)

// Looks up private Spaces APIs using dlsym.
- (BOOL)cr_initializeWorkspaceAPIs {
  static BOOL shouldInitialize = YES;
  if (shouldInitialize) {
    shouldInitialize = NO;

    NSBundle* coreGraphicsBundle =
          [NSBundle bundleWithIdentifier:@"com.apple.CoreGraphics"];
    NSString* coreGraphicsPath = [[coreGraphicsBundle bundlePath]
          stringByAppendingPathComponent:@"CoreGraphics"];
    void* coreGraphicsLibrary = dlopen([coreGraphicsPath UTF8String],
                                        RTLD_GLOBAL | RTLD_LAZY);

    if (coreGraphicsLibrary) {
      sCGSDefaultConnection =
        (CGSDefaultConnectionFunc)dlsym(coreGraphicsLibrary,
                                        "_CGSDefaultConnection");
      if (!sCGSDefaultConnection) {
        LOG(ERROR) << "Failed to lookup _CGSDefaultConnection API" << dlerror();
      }
      sCGSGetWindowWorkspace =
        (CGSGetWindowWorkspaceFunc)dlsym(coreGraphicsLibrary,
                                         "CGSGetWindowWorkspace");
      if (!sCGSGetWindowWorkspace) {
        LOG(ERROR) << "Failed to lookup CGSGetWindowWorkspace API" << dlerror();
      }
      sCGSMoveWorkspaceWindowList =
        (CGSMoveWorkspaceWindowListFunc)dlsym(coreGraphicsLibrary,
                                              "CGSMoveWorkspaceWindowList");
      if (!sCGSMoveWorkspaceWindowList) {
        LOG(ERROR) << "Failed to lookup CGSMoveWorkspaceWindowList API"
                   << dlerror();
      }
    } else {
      LOG(ERROR) << "Failed to load CoreGraphics lib" << dlerror();
    }
  }

  return sCGSDefaultConnection != NULL &&
         sCGSGetWindowWorkspace != NULL &&
         sCGSMoveWorkspaceWindowList != NULL;
}

- (BOOL)cr_workspace:(CGSWorkspaceID*)outWorkspace {
  if (![self cr_initializeWorkspaceAPIs]) {
    return NO;
  }

  // If this ASSERT fails then consider using CGSDefaultConnectionForThread()
  // instead of CGSDefaultConnection().
  DCHECK([NSThread isMainThread]);
  CGSConnectionID cid = sCGSDefaultConnection();
  CGSWindowID wid = [self windowNumber];
  CGSError err = sCGSGetWindowWorkspace(cid, wid, outWorkspace);
  return err == 0;
}

- (BOOL)cr_moveToWorkspace:(CGSWorkspaceID)workspace {
  if (![self cr_initializeWorkspaceAPIs]) {
    return NO;
  }

  // If this ASSERT fails then consider using CGSDefaultConnectionForThread()
  // instead of CGSDefaultConnection().
  DCHECK([NSThread isMainThread]);
  CGSConnectionID cid = sCGSDefaultConnection();
  CGSWindowID wid = [self windowNumber];
  // CGSSetWorkspaceForWindow doesn't seem to work for some reason.
  CGSError err = sCGSMoveWorkspaceWindowList(cid, &wid, 1, workspace);
  return err == 0;
}

@end

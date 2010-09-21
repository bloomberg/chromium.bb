// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_NSWINDOW_ADDITIONS_H_
#define CHROME_BROWSER_COCOA_NSWINDOW_ADDITIONS_H_
#pragma once

#import <Cocoa/Cocoa.h>

// ID of a Space. Starts at 1.
typedef int CGSWorkspaceID;

@interface NSWindow(ChromeAdditions)

// Gets the Space that the window is currently on. YES on success, NO on
// failure.
- (BOOL)cr_workspace:(CGSWorkspaceID*)outWorkspace;

// Moves the window to the given Space. YES on success, NO on failure.
- (BOOL)cr_moveToWorkspace:(CGSWorkspaceID)workspace;

@end

#endif  // CHROME_BROWSER_COCOA_NSWINDOW_ADDITIONS_H_

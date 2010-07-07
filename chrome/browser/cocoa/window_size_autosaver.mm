// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/cocoa/window_size_autosaver.h"

#include "chrome/browser/pref_service.h"

// If the window width stored in the prefs is smaller than this, the size is
// not restored but instead cleared from the profile -- to protect users from
// accidentally making their windows very small and then not finding them again.
const int kMinWindowWidth = 101;

// Minimum restored window height, see |kMinWindowWidth|.
const int kMinWindowHeight = 17;

@interface WindowSizeAutosaver (Private)
- (void)save:(NSNotification*)notification;
- (void)restore;
@end

@implementation WindowSizeAutosaver

- (id)initWithWindow:(NSWindow*)window
         prefService:(PrefService*)prefs
                path:(const wchar_t*)path
               state:(WindowSizeAutosaverState)state {
  if ((self = [super init])) {
    window_ = window;
    prefService_ = prefs;
    path_ = path;
    state_ = state;

    [self restore];
    [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(save:)
             name:NSWindowDidMoveNotification
           object:window_];
    [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(save:)
             name:NSWindowDidResizeNotification
           object:window_];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)save:(NSNotification*)notification {
  DictionaryValue* windowPrefs = prefService_->GetMutableDictionary(path_);
  NSRect frame = [window_ frame];
  if (state_ == kSaveWindowRect) {
    // Save the origin of the window.
    windowPrefs->SetInteger(L"left", NSMinX(frame));
    windowPrefs->SetInteger(L"right", NSMaxX(frame));
    // windows's and linux's profiles have top < bottom due to having their
    // screen origin in the upper left, while cocoa's is in the lower left. To
    // keep the top < bottom invariant, store top in bottom and vice versa.
    windowPrefs->SetInteger(L"top", NSMinY(frame));
    windowPrefs->SetInteger(L"bottom", NSMaxY(frame));
  } else if (state_ == kSaveWindowPos) {
    // Save the origin of the window.
    windowPrefs->SetInteger(L"x", frame.origin.x);
    windowPrefs->SetInteger(L"y", frame.origin.y);
  } else {
    NOTREACHED();
  }
}

- (void)restore {
  // Get the positioning information.
  DictionaryValue* windowPrefs = prefService_->GetMutableDictionary(path_);
  if (state_ == kSaveWindowRect) {
    int x1, x2, y1, y2;
    if (!windowPrefs->GetInteger(L"left", &x1) ||
        !windowPrefs->GetInteger(L"right", &x2) ||
        !windowPrefs->GetInteger(L"top", &y1) ||
        !windowPrefs->GetInteger(L"bottom", &y2)) {
      return;
    }
    if (x2 - x1 < kMinWindowWidth || y2 - y1 < kMinWindowHeight) {
      // Windows should never be very small.
      windowPrefs->Remove(L"left", NULL);
      windowPrefs->Remove(L"right", NULL);
      windowPrefs->Remove(L"top", NULL);
      windowPrefs->Remove(L"bottom", NULL);
    } else {
      [window_ setFrame:NSMakeRect(x1, y1, x2 - x1, y2 - y1) display:YES];

      // Make sure the window is on-screen.
      [window_ cascadeTopLeftFromPoint:NSZeroPoint];
    }
  } else if (state_ == kSaveWindowPos) {
    int x, y;
    if (!windowPrefs->GetInteger(L"x", &x) ||
        !windowPrefs->GetInteger(L"y", &y))
       return;  // Nothing stored.
    // Turn the origin (lower-left) into an upper-left window point.
    NSPoint upperLeft = NSMakePoint(x, y + NSHeight([window_ frame]));
    [window_ cascadeTopLeftFromPoint:upperLeft];
  } else {
    NOTREACHED();
  }
}

@end


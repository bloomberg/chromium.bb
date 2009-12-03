// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include "chrome/common/pref_service.h"

@implementation NSWindow (LocalStateAdditions)

- (void)saveWindowPositionToPrefs:(PrefService*)prefs
                         withPath:(const wchar_t*)path {
  DCHECK(prefs);
  // Save the origin of the window.
  DictionaryValue* windowPrefs = prefs->GetMutableDictionary(path);
  NSRect frame = [self frame];
  windowPrefs->SetInteger(L"x", frame.origin.x);
  windowPrefs->SetInteger(L"y", frame.origin.y);
}

- (void)restoreWindowPositionFromPrefs:(PrefService*)prefs
                              withPath:(const wchar_t*)path {
  DCHECK(prefs);
  // Get the positioning information.
  DictionaryValue* windowPrefs = prefs->GetMutableDictionary(path);
  int x = 0, y = 0;
  windowPrefs->GetInteger(L"x", &x);
  windowPrefs->GetInteger(L"y", &y);
  // Turn the origin (lower-left) into an upper-left window point.
  NSPoint upperLeft = NSMakePoint(x, y + [self frame].size.height);
  NSPoint cascadePoint = [self cascadeTopLeftFromPoint:upperLeft];
  // Cascade again to get the offset when opening new windows.
  [self cascadeTopLeftFromPoint:cascadePoint];
  // Force a save of the pref.
  [self saveWindowPositionToPrefs:prefs withPath:path];
}

@end

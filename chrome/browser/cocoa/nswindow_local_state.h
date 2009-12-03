// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class PrefService;
@class NSWindow;

// These additions to NSWindow assist in saving and restoring a window's
// position to Chromium's local state preferences.

@interface NSWindow (LocalStateAdditions)

// Saves the window's origin into the given PrefService. Caller is responsible
// for making sure |prefs| is not NULL.
- (void)saveWindowPositionToPrefs:(PrefService*)prefs
                         withPath:(const wchar_t*)path;

- (void)restoreWindowPositionFromPrefs:(PrefService*)prefs
                              withPath:(const wchar_t*)path;

@end

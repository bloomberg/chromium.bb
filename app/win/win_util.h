// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_WIN_WIN_UTIL_H_
#define APP_WIN_WIN_UTIL_H_
#pragma once

#include <windows.h>

#include <vector>

#include "base/string16.h"

class FilePath;

namespace gfx {
class Font;
class Rect;
}

namespace app {
namespace win {

// Creates a string interpretation of the time of day represented by the given
// SYSTEMTIME that's appropriate for the user's default locale.
// Format can be an empty string (for the default format), or a "format picture"
// as specified in the Windows documentation for GetTimeFormat().
string16 FormatSystemTime(const SYSTEMTIME& time,
                          const string16& format);

// Creates a string interpretation of the date represented by the given
// SYSTEMTIME that's appropriate for the user's default locale.
// Format can be an empty string (for the default format), or a "format picture"
// as specified in the Windows documentation for GetDateFormat().
string16 FormatSystemDate(const SYSTEMTIME& date,
                          const string16& format);

// Returns the long path name given a short path name. A short path name
// is a path that follows the 8.3 convention and has ~x in it. If the
// path is already a long path name, the function returns the current
// path without modification.
bool ConvertToLongPath(const string16& short_path, string16* long_path);

// Returns true if the current point is close enough to the origin point in
// space and time that it would be considered a double click.
bool IsDoubleClick(const POINT& origin,
                   const POINT& current,
                   DWORD elapsed_time);

// Returns true if the current point is far enough from the origin that it
// would be considered a drag.
bool IsDrag(const POINT& origin, const POINT& current);

// Returns true if edge |edge| (one of ABE_LEFT, TOP, RIGHT, or BOTTOM) of
// monitor |monitor| has an auto-hiding taskbar that's always-on-top.
bool EdgeHasTopmostAutoHideTaskbar(UINT edge, HMONITOR monitor);

// Duplicates a section handle from another process to the current process.
// Returns the new valid handle if the function succeed. NULL otherwise.
HANDLE GetSectionFromProcess(HANDLE section, HANDLE process, bool read_only);

// Duplicates a section handle from the current process for use in another
// process. Returns the new valid handle or NULL on failure.
HANDLE GetSectionForProcess(HANDLE section, HANDLE process, bool read_only);

// Adjusts the value of |child_rect| if necessary to ensure that it is
// completely visible within |parent_rect|.
void EnsureRectIsVisibleInRect(const gfx::Rect& parent_rect,
                               gfx::Rect* child_rect,
                               int padding);

// Returns the bounds for the monitor that contains the largest area of
// intersection with the specified rectangle.
gfx::Rect GetMonitorBoundsForRect(const gfx::Rect& rect);

// Returns true if the virtual key code is a digit coming from the numeric
// keypad (with or without NumLock on).  |extended_key| should be set to the
// extended key flag specified in the WM_KEYDOWN/UP where the |key_code|
// originated.
bool IsNumPadDigit(int key_code, bool extended_key);

// Grabs a snapshot of the designated window and stores a PNG representation
// into a byte vector.
void GrabWindowSnapshot(HWND window_handle,
                        std::vector<unsigned char>* png_representation);

// Returns whether the specified window is the current active window.
bool IsWindowActive(HWND hwnd);

// Returns whether the specified file name is a reserved name on windows.
// This includes names like "com2.zip" (which correspond to devices) and
// desktop.ini and thumbs.db which have special meaning to the windows shell.
bool IsReservedName(const string16& filename);

// A wrapper around Windows' MessageBox function. Using a Chrome specific
// MessageBox function allows us to control certain RTL locale flags so that
// callers don't have to worry about adding these flags when running in a
// right-to-left locale.
int MessageBox(HWND hwnd,
               const string16& text,
               const string16& caption,
               UINT flags);

// Returns the system set window title font.
gfx::Font GetWindowTitleFont();

// The thickness of an auto-hide taskbar in pixels.
extern const int kAutoHideTaskbarThicknessPx;

}  // namespace win
}  // namespace app

#endif  // APP_WIN_WIN_UTIL_H_

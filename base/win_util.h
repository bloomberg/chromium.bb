// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_UTIL_H__
#define BASE_WIN_UTIL_H__
#pragma once

#include <windows.h>

#include <string>

#include "base/string16.h"

struct IPropertyStore;
struct _tagpropertykey;
typedef _tagpropertykey PROPERTYKEY;

// TODO(brettw) remove this once RLZ is updated to use the new version in
// base/win/windows_version.h.
#if defined(RLZ_WIN_LIB_LIB_MUTEX_H_) || defined(RLZ_WIN_LIB_USER_KEY_H_) \
    || defined(RLZ_WIN_LIB_PROCESS_INFO_H_)
#include "base/win/windows_version.h"
#endif

namespace win_util {

void GetNonClientMetrics(NONCLIENTMETRICS* metrics);

// TODO(brettw) remove this once RLZ is updated to use the new version in
// base/win/windows_version.h.
#if defined(RLZ_WIN_LIB_LIB_MUTEX_H_) || defined(RLZ_WIN_LIB_USER_KEY_H_) \
    || defined(RLZ_WIN_LIB_PROCESS_INFO_H_)
// These must match the values in base::win!
enum WinVersion {
  WINVERSION_PRE_2000 = 0,  // Not supported
  WINVERSION_2000 = 1,      // Not supported
  WINVERSION_XP = 2,
  WINVERSION_SERVER_2003 = 3,
  WINVERSION_VISTA = 4,
  WINVERSION_2008 = 5,
  WINVERSION_WIN7 = 6,
};
inline WinVersion GetWinVersion() {
  return static_cast<WinVersion>(base::win::GetVersion());
}
#endif  // RLZ_*

// Returns the string representing the current user sid.
bool GetUserSidString(std::wstring* user_sid);

// Useful for subclassing a HWND.  Returns the previous window procedure.
WNDPROC SetWindowProc(HWND hwnd, WNDPROC wndproc);

// Pointer-friendly wrappers around Get/SetWindowLong(..., GWLP_USERDATA, ...)
// Returns the previously set value.
void* SetWindowUserData(HWND hwnd, void* user_data);
void* GetWindowUserData(HWND hwnd);

// Returns true if the shift key is currently pressed.
bool IsShiftPressed();

// Returns true if the ctrl key is currently pressed.
bool IsCtrlPressed();

// Returns true if the alt key is currently pressed.
bool IsAltPressed();

// A version of the GetClassNameW API that returns the class name in an
// std::wstring.  An empty result indicates a failure to get the class name.
std::wstring GetClassName(HWND window);

// Returns false if user account control (UAC) has been disabled with the
// EnableLUA registry flag. Returns true if user account control is enabled.
// NOTE: The EnableLUA registry flag, which is ignored on Windows XP
// machines, might still exist and be set to 0 (UAC disabled), in which case
// this function will return false. You should therefore check this flag only
// if the OS is Vista.
bool UserAccountControlIsEnabled();

// Use the Win32 API FormatMessage() function to generate a string, using
// Windows's default Message Compiled resources; ignoring the inserts.
std::wstring FormatMessage(unsigned messageid);

// Uses the last Win32 error to generate a human readable message string.
std::wstring FormatLastWin32Error();

// Sets the application id in given IPropertyStore. The function is intended
// for tagging application/chromium shortcut, browser window and jump list for
// Win7.
bool SetAppIdForPropertyStore(IPropertyStore* property_store,
                              const wchar_t* app_id);

// Adds the specified |command| using the specified |name| to the AutoRun key.
// |root_key| could be HKCU or HKLM or the root of any user hive.
bool AddCommandToAutoRun(HKEY root_key, const string16& name,
                         const string16& command);
// Removes the command specified by |name| from the AutoRun key. |root_key|
// could be HKCU or HKLM or the root of any user hive.
bool RemoveCommandFromAutoRun(HKEY root_key, const string16& name);

}  // namespace win_util

#endif  // BASE_WIN_UTIL_H__

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_UTIL_H_
#define BASE_WIN_UTIL_H_
#pragma once

#include <windows.h>

#include <string>

#include "base/string16.h"

struct IPropertyStore;
struct _tagpropertykey;
typedef _tagpropertykey PROPERTYKEY;

namespace win_util {

void GetNonClientMetrics(NONCLIENTMETRICS* metrics);

// Returns the string representing the current user sid.
bool GetUserSidString(std::wstring* user_sid);

// Returns true if the shift key is currently pressed.
bool IsShiftPressed();

// Returns true if the ctrl key is currently pressed.
bool IsCtrlPressed();

// Returns true if the alt key is currently pressed.
bool IsAltPressed();

// Returns false if user account control (UAC) has been disabled with the
// EnableLUA registry flag. Returns true if user account control is enabled.
// NOTE: The EnableLUA registry flag, which is ignored on Windows XP
// machines, might still exist and be set to 0 (UAC disabled), in which case
// this function will return false. You should therefore check this flag only
// if the OS is Vista.
bool UserAccountControlIsEnabled();

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

#endif  // BASE_WIN_UTIL_H_

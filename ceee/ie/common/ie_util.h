// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utility functions to interact with IE.

#ifndef CEEE_IE_COMMON_IE_UTIL_H_
#define CEEE_IE_COMMON_IE_UTIL_H_

#include <wtypes.h>

#include <set>

struct IWebBrowser2;

namespace ie_util {

// Returns the IWebBrowser2 interface associated to a given top level IE HWND.
// but not the one passed into not_him (which could be NULL), since there are
// some states where there are two web browser assigned to a single HWND.
HRESULT GetWebBrowserForTopLevelIeHwnd(
    HWND window, IWebBrowser2* not_him, IWebBrowser2** browser);

// NOTE: Keep these in order so callers can do things like
// "if (GetIeVersion() > IEVERSION_7) ...".  It's OK to change the values,
// though.
enum IeVersion {
  IEVERSION_UNKNOWN = -1,
  IEVERSION_PRE_IE6 = 0,  // Not supported
  IEVERSION_IE6 = 1,
  IEVERSION_IE7 = 2,
  IEVERSION_IE8 = 3,
  IEVERSION_IE9 = 4,
};

// Returns the IE version.
// Returns true/false for success/failure.
IeVersion GetIeVersion();

// Returns true if IE is in InPrivate mode.
bool GetIEIsInPrivateBrowsing();

const int kInvalidTime = -1;
// Returns average add-on load time measured by IE. Returns kInvalidTime if time
// cannot be calculated.
// @param addon_id is class id which you can find in IE add-on manager.
int GetAverageAddonLoadTimeMs(const CLSID& addon_id);

}  // namespace ie_util

#endif  // CEEE_IE_COMMON_IE_UTIL_H_

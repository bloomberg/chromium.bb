// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CHROME_FRAME_HELPER_UTIL_H_
#define CHROME_FRAME_CHROME_FRAME_HELPER_UTIL_H_

#include <windows.h>

// Compare the given class name with the known window class names for
// internet explorer browser windows.
//
// @param [in] window_to_check handle to the window to be checked
//
// @return true if the window class matches known class names for IE browser.
//
bool UtilIsWebBrowserWindow(HWND window_to_check);

// A utility function that retrieves the specified web browser COM interface
// from the web browser window. The passed in window must be the web browser
// window (class name "IEFrame" in IE6 and "TabWindowClass" in IE7)
// @param[in] window The web browser window whose COM object is to be fetched
// @param[in] iid    The IID of the interface to be fetched
// @param[out] web_browser_object
//                   The requested interface pointer to the web browser object
//                   is returned in this variable upon success
//
HRESULT UtilGetWebBrowserObjectFromWindow(HWND window,
                                          REFIID iid,
                                          void** web_browser_object);

// Checks to see whether the passed in window is of the class specified.
// of the heirarchy list
// @param hwnd_to_match        [in]  The handle of the window that is to be
//                                   matched.
// @param window_class         [in]  The window class to match against.
//
bool IsWindowOfClass(HWND hwnd_to_match, const wchar_t* window_class);

// Returns true if the current process name is process_name, false otherwise.
bool IsNamedProcess(const wchar_t* process_name);

#endif  // CHROME_FRAME_CHROME_FRAME_HELPER_UTIL_H_

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

// Returns true if window has the name |window_name|, false otherwise.
bool IsNamedWindow(HWND window, const wchar_t* window_name);

//
// This function recursively enumerates all windows from a given starting point
// and searches for the first occurrence of a window matching
// the class name and window name passed in. We use the EnumChildWindows API
// to search for the window.
// @note The FindWindowEx function does something similar, however, it goes
// only one level deep and does not recurse descendants.
// @param parent [in] The HWND of the window from which to start looking. If
//                    this is NULL then enumerate all windows on the desktop.
// @param class_name [in, optional] The class name of the matching window
// @param window_name [in, optional] The window text of the matching window.
//                                   At least one of class_name and
//                                   window_name must be non-NULL.
// @param thread_id_to_match [in] This parameter will be used to match
//                                particular thread id for window.
// @param process_id_to_match [in] This parameter will be used to match
//                                 particular process id for window.
// @return The window handle of the matching window, if found, or NULL.
//
HWND RecurseFindWindow(HWND parent,
                       const wchar_t* class_name,
                       const wchar_t* window_name,
                       DWORD thread_id_to_match,
                       DWORD process_id_to_match);

#endif  // CHROME_FRAME_CHROME_FRAME_HELPER_UTIL_H_

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities for windows (as in HWND).

#ifndef CEEE_COMMON_WINDOW_UTILS_H_
#define CEEE_COMMON_WINDOW_UTILS_H_

#include <windows.h>

#include <set>
#include <string>

namespace window_utils {

// Returns true if the given window is of the specified window class.
bool IsWindowClass(HWND window, const std::wstring& window_class);

// Returns the top-level window that is an ancestor of the provided window.
// Note that this will return the window passed in if it has no parent, or if
// it is not a window.
HWND GetTopLevelParent(HWND window);

// Returns true if we are currently running in the given HWND's thread.
bool IsWindowThread(HWND window);

// Returns true if <p> window is not associated to any thread anymore.
bool WindowHasNoThread(HWND window);

// Returns true if we could find a descendent of the ancestor window with the
// specified window class, and visible if @p only_visible is true.
// Also returns the HWND of the descendent in @p descendent.
//
// @param ancestor The ancestor window to parse the descendent hierarchy of.
//    As documented in EnumChildWindows, if ancestor is NULL, we only check
//    top level windows.
// @param window_class The window class we are looking for.
// @param only_visible Specify whether we only want a visible descendent
//    (if true) or don't care about the visibility (if false).
// @param only_accessible Specify if windows associated with processes not
//    accessible from the current process are to be filtered out. Particularly
//    relevant if ancestor == NULL.
// @param descendent Where to return the handle of the descendent window,
//    or NULL if you don't need the handle.
bool FindDescendentWindowEx(HWND ancestor,
                            const std::wstring& window_class,
                            bool only_visible,
                            bool only_accessible,
                            HWND* descendent);

// Abbreviated version of the above, provided for backward compatibility.
// Assumes only_accessible == true.
bool FindDescendentWindow(HWND ancestor,
                          const std::wstring& window_class,
                          bool only_visible,
                          HWND* descendent);


// Finds all the top level windows. The @p top_level_windows are filtered
// by the provided @p window_class name if it's not empty. An empty
// @p window_class name means that we are looking for all children.
// @p only_visible means that windows associated with processes not accessible
//    from the current process are to be filtered out.
void FindTopLevelWindowsEx(const std::wstring& window_class,
                           bool only_visible,
                           std::set<HWND>* top_level_windows);

// Abbreviated version of the above, provided for backward compatibility.
// Assumes only_visible == true.
void FindTopLevelWindows(const std::wstring& window_class,
                         std::set<HWND>* top_level_windows);

}  // namespace window_utils

#endif  // CEEE_COMMON_WINDOW_UTILS_H_

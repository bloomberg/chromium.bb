// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Constants for things in the Windows environment (window class names,
// process names, etc.)

#ifndef CEEE_COMMON_WINDOWS_CONSTANTS_H_
#define CEEE_COMMON_WINDOWS_CONSTANTS_H_

namespace windows {

// Module names

// Windows Explorer.
extern const wchar_t kExplorerModuleName[];

// Window class names

// The desktop window.
extern const wchar_t kDesktopWindowClass[];

// The IE window containing the whole tab.
extern const wchar_t kIeTabWindowClass[];

// The top-level IE window.
extern const wchar_t kIeFrameWindowClass[];

// A hidden top-level IE window.
extern const wchar_t kHiddenIeFrameWindowClass[];

// The parent window of the tab content window.
extern const wchar_t kIeTabContentParentWindowClass[];

// The IE window containing the tab content.
extern const wchar_t kIeTabContentWindowClass[];

// The shell's tray window.
extern const wchar_t kShellTrayWindowClass[];

// The shell's notification window.
extern const wchar_t kTrayNotifyWindowClass[];

}  // namespace windows

#endif  // CEEE_COMMON_WINDOWS_CONSTANTS_H_

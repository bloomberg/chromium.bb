// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Constants for things in the Windows environment (window class names,
// process names, etc.)

#include "ceee/common/windows_constants.h"

namespace windows {

const wchar_t kExplorerModuleName[] = L"explorer.exe";
const wchar_t kDesktopWindowClass[] = L"#32769";
const wchar_t kIeTabWindowClass[] = L"TabWindowClass";
const wchar_t kIeFrameWindowClass[] = L"IEFrame";
const wchar_t kHiddenIeFrameWindowClass[] = L"Internet Explorer_Hidden";
const wchar_t kIeTabContentParentWindowClass[] = L"Shell DocObject View";
const wchar_t kIeTabContentWindowClass[] = L"Internet Explorer_Server";
const wchar_t kShellTrayWindowClass[] = L"Shell_TrayWnd";
const wchar_t kTrayNotifyWindowClass[] = L"TrayNotifyWnd";

}  // namespace windows

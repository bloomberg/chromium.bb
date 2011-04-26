// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_VIRTUAL_DRIVER_WIN_HELPERS_H_
#define CLOUD_PRINT_VIRTUAL_DRIVER_WIN_HELPERS_H_
#pragma once

#include <windows.h>

namespace cloud_print {

// Convert an HRESULT to a localized string and display it in a message box.
void DisplayWindowsMessage(HWND hwnd, HRESULT message_id);

// Similar to the Windows API call GetLastError but returns an HRESULT.
HRESULT GetLastHResult();
}

#endif  // CLOUD_PRINT_VIRTUAL_DRIVER_WIN_HELPERS_H_




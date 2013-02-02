// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_VIRTUAL_DRIVER_WIN_VIRTUAL_DRIVER_HELPERS_H_
#define CLOUD_PRINT_VIRTUAL_DRIVER_WIN_VIRTUAL_DRIVER_HELPERS_H_

#include <windows.h>

#include "base/string16.h"

namespace base {
class FilePath;
}

namespace cloud_print {

// Returns TRUE if the current OS is 64 bit.
bool IsSystem64Bit();

// Convert an HRESULT to a localized string and display it in a message box.
void DisplayWindowsMessage(HWND hwnd,
                           HRESULT message_id,
                           const string16 &caption);

// Similar to the Windows API call GetLastError but returns an HRESULT.
HRESULT GetLastHResult();

// Returns the correct port monitor DLL file name for the current machine.
string16 GetPortMonitorDllName();

// Gets the standard install path for "version 3" print drivers.
HRESULT GetPrinterDriverDir(base::FilePath* path);

// Retrieves a string from the string table of the module that contains the
// calling code.
string16 LoadLocalString(DWORD string_id);

}  // namespace cloud_print

#endif  // CLOUD_PRINT_VIRTUAL_DRIVER_WIN_VIRTUAL_DRIVER_HELPERS_H_




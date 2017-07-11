// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONFLICTS_ENUMERATE_INPUT_METHOD_EDITORS_WIN_H_
#define CHROME_BROWSER_CONFLICTS_ENUMERATE_INPUT_METHOD_EDITORS_WIN_H_

#include <stdint.h>

#include "base/callback_forward.h"

namespace base {
class FilePath;
}

// The path to the registry key where IMEs are registered.
extern const wchar_t kImeRegistryKey[];

// A format string for generating paths to COM class in-proc server keys under
// HKEY_CLASSES_ROOT.
extern const wchar_t kClassIdRegistryKeyFormat[];

// Finds third-party IMEs (Input Method Editor) installed on the computer by
// enumerating the registry. In addition to the file path, the SizeOfImage and
// TimeDateStamp of the module is returned via the callback.
using OnImeEnumeratedCallback =
    base::RepeatingCallback<void(const base::FilePath&, uint32_t, uint32_t)>;
void EnumerateInputMethodEditors(OnImeEnumeratedCallback callback);

#endif  // CHROME_BROWSER_CONFLICTS_ENUMERATE_INPUT_METHOD_EDITORS_WIN_H_

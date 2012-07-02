// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_ERROR_DIALOG_H_
#define CHROME_BROWSER_PRINTING_PRINT_ERROR_DIALOG_H_
#pragma once

#include "ui/gfx/native_widget_types.h"

namespace chrome {

// Shows a window-modal error that printing failed for some unknown reason.
void ShowPrintErrorDialog(gfx::NativeWindow parent);

}  // namespace chrome

#endif  // CHROME_BROWSER_PRINTING_PRINT_ERROR_DIALOG_H_

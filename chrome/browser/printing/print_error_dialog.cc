// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_error_dialog.h"

#include "chrome/browser/ui/simple_message_box.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chrome {

void ShowPrintErrorDialog(gfx::NativeWindow parent) {
  browser::ShowMessageBox(
      parent,
      l10n_util::GetStringUTF16(IDS_PRINT_SPOOL_FAILED_TITLE_TEXT),
      l10n_util::GetStringUTF16(IDS_PRINT_SPOOL_FAILED_ERROR_TEXT),
      browser::MESSAGE_BOX_TYPE_WARNING);
}

}  // namespace chrome

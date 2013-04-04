// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/apps_metro_handler_win.h"

#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chrome {

bool VerifySwitchToMetroForApps(gfx::NativeWindow parent_window) {
  if (!extensions::ShellWindowRegistry::IsShellWindowRegisteredInAnyProfile(
          ShellWindow::WINDOW_TYPE_DEFAULT)) {
    return true;
  }

  MessageBoxResult result = ShowMessageBox(
      parent_window,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
      l10n_util::GetStringUTF16(IDS_WIN8_PROMPT_TO_CLOSE_APPS_FOR_METRO),
      MESSAGE_BOX_TYPE_OK_CANCEL);

  return result == MESSAGE_BOX_RESULT_YES;
}

}  // namespace chrome

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/apps/apps_metro_handler_win.h"

#include "apps/app_window.h"
#include "apps/app_window_registry.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

bool VerifyMetroSwitchForApps(gfx::NativeWindow parent_window,
                              int win8_restart_command_id) {
  DCHECK(win8_restart_command_id == IDC_WIN8_DESKTOP_RESTART ||
      win8_restart_command_id == IDC_WIN8_METRO_RESTART);
  if (!apps::AppWindowRegistry::IsAppWindowRegisteredInAnyProfile(
           apps::AppWindow::WINDOW_TYPE_DEFAULT)) {
    return true;
  }

  int string_id = win8_restart_command_id == IDC_WIN8_METRO_RESTART ?
      IDS_WIN8_PROMPT_TO_CLOSE_APPS_FOR_METRO :
      IDS_WIN8_PROMPT_TO_CLOSE_APPS_FOR_DESKTOP;
  chrome::MessageBoxResult result = chrome::ShowMessageBox(
      parent_window,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
      l10n_util::GetStringUTF16(string_id),
      chrome::MESSAGE_BOX_TYPE_OK_CANCEL);

  return result == chrome::MESSAGE_BOX_RESULT_YES;
}

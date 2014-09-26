// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/apps/apps_metro_handler_win.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_window_registry_util.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/app_window/app_window.h"
#include "ui/base/l10n/l10n_util.h"

bool VerifyASHSwitchForApps(gfx::NativeWindow parent_window,
                            int win_restart_command_id) {
  DCHECK(win_restart_command_id == IDC_WIN_DESKTOP_RESTART ||
      win_restart_command_id == IDC_WIN8_METRO_RESTART ||
      win_restart_command_id == IDC_WIN_CHROMEOS_RESTART);
  if (!AppWindowRegistryUtil::IsAppWindowRegisteredInAnyProfile(
           extensions::AppWindow::WINDOW_TYPE_DEFAULT)) {
    return true;
  }

  int string_id = 0;
  switch (win_restart_command_id) {
    case IDC_WIN8_METRO_RESTART:
      string_id = IDS_WIN8_PROMPT_TO_CLOSE_APPS_FOR_METRO;
      break;
    case IDC_WIN_CHROMEOS_RESTART:
      string_id = IDS_WIN_PROMPT_TO_CLOSE_APPS_FOR_CHROMEOS;
      break;
    default:
      string_id = IDS_WIN_PROMPT_TO_CLOSE_APPS_FOR_DESKTOP;
      break;
  }
  chrome::MessageBoxResult result = chrome::ShowMessageBox(
      parent_window,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
      l10n_util::GetStringUTF16(string_id),
      chrome::MESSAGE_BOX_TYPE_OK_CANCEL);

  return result == chrome::MESSAGE_BOX_RESULT_YES;
}

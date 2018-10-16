// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/chrome_select_file_dialog_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "ui/shell_dialogs/execute_select_file_win.h"
#include "ui/shell_dialogs/select_file_dialog_win.h"
#include "ui/shell_dialogs/select_file_policy.h"

ChromeSelectFileDialogFactory::ChromeSelectFileDialogFactory() = default;

ChromeSelectFileDialogFactory::~ChromeSelectFileDialogFactory() = default;

ui::SelectFileDialog* ChromeSelectFileDialogFactory::Create(
    ui::SelectFileDialog::Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) {
  // TODO(pmonette): Re-implement the OOP file dialogs using the ShellUtilWin
  // interface.
  return ui::CreateWinSelectFileDialog(
      listener, std::move(policy), base::BindRepeating(&ui::ExecuteSelectFile));
}

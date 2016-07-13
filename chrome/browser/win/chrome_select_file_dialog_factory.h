// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_CHROME_SELECT_FILE_DIALOG_FACTORY_H_
#define CHROME_BROWSER_WIN_CHROME_SELECT_FILE_DIALOG_FACTORY_H_

#include "base/macros.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"

// Implements a Select File dialog that delegates to a Metro file picker on
// Metro and to a utility process otherwise. The utility process is used in
// order to isolate the Chrome browser process from potential instability
// caused by Shell extension modules loaded by GetOpenFileName.
class ChromeSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 public:
  ChromeSelectFileDialogFactory();
  ~ChromeSelectFileDialogFactory() override;

  // ui::SelectFileDialogFactory implementation
  ui::SelectFileDialog* Create(ui::SelectFileDialog::Listener* listener,
                               ui::SelectFilePolicy* policy) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeSelectFileDialogFactory);
};

#endif  // CHROME_BROWSER_WIN_CHROME_SELECT_FILE_DIALOG_FACTORY_H_

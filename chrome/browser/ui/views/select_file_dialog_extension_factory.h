// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SELECT_FILE_DIALOG_EXTENSION_FACTORY_H_
#define CHROME_BROWSER_UI_VIEWS_SELECT_FILE_DIALOG_EXTENSION_FACTORY_H_

#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"

class SelectFileDialogExtensionFactory : public ui::SelectFileDialogFactory {
 public:
  SelectFileDialogExtensionFactory();
  virtual ~SelectFileDialogExtensionFactory();

 private:
  // Override from ui::SelectFileDialogFactory:
  virtual ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      ui::SelectFilePolicy* policy) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(SelectFileDialogExtensionFactory);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SELECT_FILE_DIALOG_EXTENSION_FACTORY_H_

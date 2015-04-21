// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_DEVICE_PERMISSIONS_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_DEVICE_PERMISSIONS_DIALOG_VIEW_H_

#include "extensions/browser/api/device_permissions_prompt.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class TableView;
}

class DevicePermissionsTableModel;

// Displays a device permissions selector prompt as a modal dialog constrained
// to the window/tab displaying the given web contents.
class DevicePermissionsDialogView : public views::DialogDelegateView {
 public:
  DevicePermissionsDialogView(
      scoped_refptr<extensions::DevicePermissionsPrompt::Prompt> prompt);
  ~DevicePermissionsDialogView() override;

  // views::DialogDelegateView:
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  void DeleteDelegate() override;
  bool Accept() override;
  gfx::Size GetPreferredSize() const override;

 private:
  scoped_refptr<extensions::DevicePermissionsPrompt::Prompt> prompt_;

  // Displays the list of devices.
  views::TableView* table_view_;
  scoped_ptr<DevicePermissionsTableModel> table_model_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_DEVICE_PERMISSIONS_DIALOG_VIEW_H_

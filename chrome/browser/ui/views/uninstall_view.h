// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_UNINSTALL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_UNINSTALL_VIEW_H_
#pragma once

#include <map>

#include "base/string16.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Checkbox;
class Combobox;
class Label;
}

// UninstallView implements the dialog that confirms Chrome uninstallation
// and asks whether to delete Chrome profile. Also if currently Chrome is set
// as default browser, it asks users whether to set another browser as default.
class UninstallView : public views::ButtonListener,
                      public views::DialogDelegateView,
                      public ui::ComboboxModel {
 public:
  explicit UninstallView(int* user_selection);
  virtual ~UninstallView();

  // Overridden form views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // Overridden from views::DialogDelegateView:
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual string16 GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;

  // Overridden from ui::ComboboxModel:
  virtual int GetItemCount() OVERRIDE;
  virtual string16 GetItemAt(int index) OVERRIDE;

 private:
  // Initializes the controls on the dialog.
  void SetupControls();

  views::Label* confirm_label_;
  views::Checkbox* delete_profile_;
  views::Checkbox* change_default_browser_;
  views::Combobox* browsers_combo_;
  typedef std::map<std::wstring, std::wstring> BrowsersMap;
  scoped_ptr<BrowsersMap> browsers_;
  int& user_selection_;

  DISALLOW_COPY_AND_ASSIGN(UninstallView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_UNINSTALL_VIEW_H_

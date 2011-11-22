// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_PASSWORD_CHANGED_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_PASSWORD_CHANGED_VIEW_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "ui/views/window/dialog_delegate.h"
#include "views/controls/button/button.h"
#include "views/controls/textfield/textfield_controller.h"
#include "views/view.h"

namespace views {
class Button;
class Label;
class RadioButton;
class Textfield;
}  // namespace views

namespace chromeos {

// A dialog box that is shown when password change was detected.
// User is presented with an option to sync all settings or
// enter old password and sync only delta.
class PasswordChangedView : public views::DialogDelegateView,
                            public views::ButtonListener,
                            public views::TextfieldController {
 public:
  // Delegate class to get notifications from the view.
  class Delegate {
  public:
    virtual ~Delegate() {}

    // User provided |old_password|, decrypt homedir and sync only delta.
    virtual void RecoverEncryptedData(const std::string& old_password) = 0;

    // Ignores password change and forces full sync.
    virtual void ResyncEncryptedData() = 0;
  };

  PasswordChangedView(Delegate* delegate, bool full_sync_disabled);
  virtual ~PasswordChangedView() {}

  // views::DialogDelegate:
  virtual bool Accept() OVERRIDE;
  virtual int GetDialogButtons() const OVERRIDE;

  // views::WidgetDelegate:
  virtual View* GetInitiallyFocusedView() OVERRIDE;
  virtual bool IsModal() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;

  // views::View:
  virtual string16 GetWindowTitle() const OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // views::TextfieldController:
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const views::KeyEvent& keystroke) OVERRIDE;
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents) OVERRIDE {}

 protected:
  // views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) OVERRIDE;

 private:
  // Called when dialog is accepted.
  bool ExitDialog();

  // Initialize view layout.
  void Init();

  // Screen controls.
  views::Label* title_label_;
  views::Label* description_label_;
  views::RadioButton* full_sync_radio_;
  views::RadioButton* delta_sync_radio_;
  views::Textfield* old_password_field_;

  // Notifications receiver.
  Delegate* delegate_;

  // Whether full sync option is disabled.
  bool full_sync_disabled_;

  DISALLOW_COPY_AND_ASSIGN(PasswordChangedView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_PASSWORD_CHANGED_VIEW_H_

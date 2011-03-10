// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_PASSWORD_CHANGED_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_PASSWORD_CHANGED_VIEW_H_
#pragma once

#include <string>

#include "views/controls/button/button.h"
#include "views/controls/textfield/textfield_controller.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"

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
class PasswordChangedView : public views::View,
                            public views::DialogDelegate,
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
  virtual bool Accept();
  virtual int GetDialogButtons() const;

  // views::WindowDelegate:
  virtual View* GetInitiallyFocusedView();
  virtual bool IsModal() const { return true; }
  virtual views::View* GetContentsView() { return this; }

  // views::View:
  virtual std::wstring GetWindowTitle() const;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event);

  // views::TextfieldController:
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const views::KeyEvent& keystroke) {
    return false;
  }
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents) {}

 protected:
  // views::View:
  virtual gfx::Size GetPreferredSize();
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

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

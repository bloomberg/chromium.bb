// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PASSWORD_DIALOG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_PASSWORD_DIALOG_VIEW_H_

#include <string>

#include "base/string16.h"
#include "views/window/dialog_delegate.h"

namespace views {
class Textfield;
class View;
class Window;
}

namespace chromeos {

// Delegate implemented by caller of PasswordDialogView to handle the user
// interacting with the dialog box.
class PasswordDialogDelegate {
 public:
  // Called when user clicks on cancel button.
  // Return whether or not to allow password dialog to close.
  virtual bool OnPasswordDialogCancel() = 0;

  // Called when user clicks on ok with a password.
  // Return whether or not to allow password dialog to close.
  virtual bool OnPasswordDialogAccept(const std::string& ssid,
                                      const string16& password) = 0;
};

// A dialog box for showing a password textfield.
class PasswordDialogView : public views::View,
                           public views::DialogDelegate {
 public:
  explicit PasswordDialogView(PasswordDialogDelegate* delegate,
                              const std::string& ssid);
  virtual ~PasswordDialogView() {}

  // views::DialogDelegate methods.
  virtual bool Cancel();
  virtual bool Accept();
  virtual std::wstring GetWindowTitle() const;

  // views::WindowDelegate method.
  virtual bool IsModal() const { return true; }
  virtual views::View* GetContentsView() { return this; }

  // views::View overrides.
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

 protected:
  virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                    views::View* child);

 private:
  void Init();

  // Used for call back to delegate that password has been entered.
  PasswordDialogDelegate* delegate_;

  // The ssid we are requesting the password for.
  std::string ssid_;

  // Combobox and its corresponding model.
  views::Textfield* password_textfield_;

  DISALLOW_COPY_AND_ASSIGN(PasswordDialogView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PASSWORD_DIALOG_VIEW_H_

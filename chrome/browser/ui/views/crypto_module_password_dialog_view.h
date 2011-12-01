// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CRYPTO_MODULE_PASSWORD_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CRYPTO_MODULE_PASSWORD_DIALOG_VIEW_H_
#pragma once

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/crypto_module_password_dialog.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Textfield;
class Label;
}

namespace browser {

// CryptoModulePasswordDialogView
// Dialog view for crypto module password interaction.
/////////////////////////////////////////////////////////////////////////
class CryptoModulePasswordDialogView : public views::DialogDelegateView,
                                       public views::TextfieldController {
 public:
  CryptoModulePasswordDialogView(
      const std::string& slot_name,
      browser::CryptoModulePasswordReason reason,
      const std::string& server,
      const base::Callback<void(const char*)>& callback);

  virtual ~CryptoModulePasswordDialogView();

  // views::DialogDelegate:
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual string16 GetDialogButtonLabel(
      ui::DialogButton button) const OVERRIDE;


  // views::WidgetDelegate:
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;
  virtual bool IsModal() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;

  // views::View:
  virtual string16 GetWindowTitle() const OVERRIDE;

  // views::TextfieldController:
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const views::KeyEvent& keystroke) OVERRIDE;
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents) OVERRIDE;

 private:
  // Initialize views and layout.
  void Init(const std::string& server,
            const std::string& slot_name,
            browser::CryptoModulePasswordReason reason);

  views::Label* title_label_;
  views::Label* reason_label_;
  views::Label* password_label_;
  views::Textfield* password_entry_;

  const base::Callback<void(const char*)> callback_;
  FRIEND_TEST_ALL_PREFIXES(CryptoModulePasswordDialogViewTest, TestAccept);

  DISALLOW_COPY_AND_ASSIGN(CryptoModulePasswordDialogView);
};

}  // namespace browser

#endif  // CHROME_BROWSER_UI_VIEWS_CRYPTO_MODULE_PASSWORD_DIALOG_VIEW_H_

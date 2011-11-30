// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/crypto_module_password_dialog.h"
#include "chrome/browser/ui/views/crypto_module_password_dialog_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/textfield/textfield.h"

std::string kSlotName = "slot";
std::string kServer = "server";

namespace browser {
CryptoModulePasswordReason kReason = kCryptoModulePasswordKeygen;

class CryptoModulePasswordDialogViewTest : public testing::Test {
 public:
  CryptoModulePasswordDialogViewTest() {}
  ~CryptoModulePasswordDialogViewTest() {}
  void Capture(const char* text) {
    text_ = text;
  }
  void CreateDialogCrypto(const CryptoModulePasswordCallback& callback) {
    dialog_.reset(new CryptoModulePasswordDialogView(
        kSlotName, kReason, kServer, callback));
  }
  browser::CryptoModulePasswordCallback* callback_;
  std::string text_;
  scoped_ptr<CryptoModulePasswordDialogView> dialog_;
};

TEST_F(CryptoModulePasswordDialogViewTest, TestAccept) {
  browser::CryptoModulePasswordCallback cb(
      base::Bind(&browser::CryptoModulePasswordDialogViewTest::Capture,
                 base::Unretained(this)));
  CreateDialogCrypto(cb);
  EXPECT_EQ(dialog_->password_entry_, dialog_->GetInitiallyFocusedView());
  EXPECT_TRUE(dialog_->IsModal());
  const std::string kPassword = "diAl0g";
  dialog_->password_entry_->SetText(UTF8ToUTF16(kPassword));
  EXPECT_TRUE(dialog_->Accept());
  EXPECT_EQ(kPassword, text_);
  const string16 empty;
  EXPECT_EQ(empty, dialog_->password_entry_->text());
}
}  // namespace browser

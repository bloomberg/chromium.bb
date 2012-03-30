// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/one_click_signin_dialog_gtk.h"

#include <gtk/gtk.h>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::_;

class OneClickSigninDialogGtkTest : public ::testing::Test {
 public:
  OneClickSigninDialogGtkTest()
      : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        accept_callback_(
            base::Bind(&OneClickSigninDialogGtkTest::OnAccept,
                       weak_ptr_factory_.GetWeakPtr())),
        dialog_(new OneClickSigninDialogGtk(NULL, accept_callback_)) {
  }

  MOCK_METHOD1(OnAccept, void(bool));

 protected:
  base::WeakPtrFactory<OneClickSigninDialogGtkTest> weak_ptr_factory_;
  OneClickAcceptCallback accept_callback_;
  // Owns itself.
  OneClickSigninDialogGtk* dialog_;
};

// Test that the dialog doesn't call its accept callback if it is
// cancelled.
TEST_F(OneClickSigninDialogGtkTest, RunAndCancel) {
  EXPECT_CALL(*this, OnAccept(_)).Times(0);

  dialog_->SendResponseForTest(GTK_RESPONSE_CLOSE);
}

// Test that the dialog calls its accept callback with
// use_default_settings=true if it is accepted without changing the
// corresponding checkbox state.
TEST_F(OneClickSigninDialogGtkTest, RunAndAcceptWithDefaultSettings) {
  EXPECT_CALL(*this, OnAccept(true));

  dialog_->SendResponseForTest(GTK_RESPONSE_ACCEPT);
}

// Test that the dialog calls its accept callback with
// use_default_settings=false if it is accepted after unchecking the
// corresponding checkbox.
TEST_F(OneClickSigninDialogGtkTest, RunAndAcceptWithoutDefaultSettings) {
  EXPECT_CALL(*this, OnAccept(false));

  dialog_->SetUseDefaultSettingsForTest(false);
  dialog_->SendResponseForTest(GTK_RESPONSE_ACCEPT);
}

}  // namespace

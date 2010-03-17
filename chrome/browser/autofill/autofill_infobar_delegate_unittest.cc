// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/autofill/autofill_infobar_delegate.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/test_tab_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

class SkBitmap;

namespace {

class MockAutoFillManager : public AutoFillManager {
 public:
  explicit MockAutoFillManager(TabContents* tab_contents)
      : AutoFillManager(tab_contents),
        responded_(false),
        accepted_(false) {}

  virtual void OnInfoBarClosed() {
    EXPECT_FALSE(responded_);
    responded_ = true;
    accepted_ = true;
  }

  virtual void OnInfoBarAccepted() {
    EXPECT_FALSE(responded_);
    responded_ = true;
    accepted_ = true;
  }

  virtual void OnInfoBarCancelled() {
    EXPECT_FALSE(responded_);
    responded_ = true;
    accepted_ = false;
  }

  bool responded() {
    return responded_;
  }

  bool accepted() {
    return accepted_;
  }

 private:
  // True if we have gotten some sort of response.
  bool responded_;

  // True if we have gotten an Accept response. Meaningless if |responded_| is
  // not true.
  bool accepted_;

  DISALLOW_COPY_AND_ASSIGN(MockAutoFillManager);
};

class AutoFillInfoBarDelegateTest : public RenderViewHostTestHarness {
 public:
  AutoFillInfoBarDelegateTest() {}

  virtual void SetUp() {
    RenderViewHostTestHarness::SetUp();
    profile()->CreateWebDataService(true);
    profile()->CreatePersonalDataManager();
    autofill_manager_.reset(new MockAutoFillManager(contents()));
    infobar_.reset(new AutoFillInfoBarDelegate(NULL, autofill_manager_.get()));
  }

 protected:
  scoped_ptr<MockAutoFillManager> autofill_manager_;
  scoped_ptr<AutoFillInfoBarDelegate> infobar_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoFillInfoBarDelegateTest);
};

TEST_F(AutoFillInfoBarDelegateTest, ShouldExpire) {
  NavigationController::LoadCommittedDetails details;
  EXPECT_FALSE(infobar_->ShouldExpire(details));
}

TEST_F(AutoFillInfoBarDelegateTest, InfoBarClosed) {
  infobar_->InfoBarClosed();
  EXPECT_TRUE(autofill_manager_->responded());
  EXPECT_TRUE(autofill_manager_->accepted());
}

TEST_F(AutoFillInfoBarDelegateTest, GetMessageText) {
  std::wstring text = l10n_util::GetString(IDS_AUTOFILL_INFOBAR_TEXT);
  EXPECT_EQ(text, infobar_->GetMessageText());
}

TEST_F(AutoFillInfoBarDelegateTest, GetIcon) {
  SkBitmap* icon = infobar_->GetIcon();
  EXPECT_NE(static_cast<SkBitmap*>(NULL), icon);
}

TEST_F(AutoFillInfoBarDelegateTest, GetButtons) {
  int buttons =
      ConfirmInfoBarDelegate::BUTTON_OK | ConfirmInfoBarDelegate::BUTTON_CANCEL;
  EXPECT_EQ(buttons, infobar_->GetButtons());
}

TEST_F(AutoFillInfoBarDelegateTest, GetButtonLabel) {
  std::wstring accept = l10n_util::GetString(IDS_AUTOFILL_INFOBAR_ACCEPT);
  EXPECT_EQ(accept,
      infobar_->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK));

  std::wstring deny = l10n_util::GetString(IDS_AUTOFILL_INFOBAR_DENY);
  EXPECT_EQ(deny,
      infobar_->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL));

  // NOTREACHED if neither BUTTON_OK or BUTTON_CANCEL are passed in.
  ASSERT_DEBUG_DEATH(
      infobar_->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_NONE), "");
}

TEST_F(AutoFillInfoBarDelegateTest, Accept) {
  infobar_->Accept();
  EXPECT_TRUE(autofill_manager_->responded());
  EXPECT_TRUE(autofill_manager_->accepted());
}

TEST_F(AutoFillInfoBarDelegateTest, Cancel) {
  infobar_->Cancel();
  EXPECT_TRUE(autofill_manager_->responded());
  EXPECT_FALSE(autofill_manager_->accepted());
}

}  // namespace

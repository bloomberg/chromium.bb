// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_dialog_views.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_footer_panel.h"
#include "chrome/test/base/testing_profile.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#if defined(OS_CHROMEOS)
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "components/arc/arc_session_runner.h"
#include "components/arc/test/fake_arc_session.h"
#endif

namespace {

const char kTestExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

}  // namespace

class AppInfoDialogAshTest : public ash::test::AshTestBase {
 public:
  AppInfoDialogAshTest()
      : extension_environment_(base::MessageLoopForUI::current()) {}

  // Overridden from testing::Test:
  void SetUp() override {
    ash::test::AshTestBase::SetUp();
#if defined(OS_CHROMEOS)
    arc::ArcSessionManager::DisableUIForTesting();
    arc_session_manager_ = base::MakeUnique<arc::ArcSessionManager>(
        base::MakeUnique<arc::ArcSessionRunner>(
            base::Bind(arc::FakeArcSession::Create)));
    arc_session_manager_->OnPrimaryUserProfilePrepared(
        extension_environment_.profile());
#endif
    widget_ = views::DialogDelegate::CreateDialogWidget(
        new views::DialogDelegateView(), CurrentContext(), NULL);
    dialog_ = new AppInfoDialog(
        widget_->GetNativeWindow(), extension_environment_.profile(),
        extension_environment_.MakePackagedApp(kTestExtensionId, true).get());
    widget_->GetContentsView()->AddChildView(dialog_);
    widget_->Show();
  }

  void TearDown() override {
    widget_->CloseNow();
#if defined(OS_CHROMEOS)
    if (arc_session_manager_)
      arc_session_manager_->Shutdown();
#endif
    ash::test::AshTestBase::TearDown();
  }

 protected:
  extensions::TestExtensionEnvironment extension_environment_;
  views::Widget* widget_ = nullptr;
  AppInfoDialog* dialog_ = nullptr;  // Owned by |widget_|'s views hierarchy.
#if defined(OS_CHROMEOS)
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(AppInfoDialogAshTest);
};

// Tests that the pin/unpin button is focused after unpinning/pinning. This is
// to verify regression in crbug.com/428704 is fixed.
TEST_F(AppInfoDialogAshTest, PinButtonsAreFocusedAfterPinUnpin) {
  AppInfoFooterPanel* dialog_footer =
      static_cast<AppInfoFooterPanel*>(dialog_->dialog_footer_);
  views::View* pin_button = dialog_footer->pin_to_shelf_button_;
  views::View* unpin_button = dialog_footer->unpin_from_shelf_button_;

  pin_button->RequestFocus();
  EXPECT_TRUE(pin_button->visible());
  EXPECT_FALSE(unpin_button->visible());
  EXPECT_TRUE(pin_button->HasFocus());

  dialog_footer->SetPinnedToShelf(true);
  EXPECT_FALSE(pin_button->visible());
  EXPECT_TRUE(unpin_button->visible());
  EXPECT_TRUE(unpin_button->HasFocus());

  dialog_footer->SetPinnedToShelf(false);
  EXPECT_TRUE(pin_button->visible());
  EXPECT_FALSE(unpin_button->visible());
  EXPECT_TRUE(pin_button->HasFocus());
}

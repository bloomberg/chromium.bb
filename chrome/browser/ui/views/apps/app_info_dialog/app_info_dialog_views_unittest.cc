// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_dialog_views.h"

#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/extension_system.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

const char kTestExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestOtherExtensionId[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

}  // namespace

class AppInfoDialogViewsTest : public ash::test::AshTestBase,
                               public views::WidgetObserver {
 public:
  AppInfoDialogViewsTest()
      : extension_environment_(base::MessageLoopForUI::current()) {}

  // Overridden from testing::Test:
  void SetUp() override {
    ash::test::AshTestBase::SetUp();
    widget_ = views::DialogDelegate::CreateDialogWidget(
        new views::DialogDelegateView(), CurrentContext(), NULL);
    widget_->AddObserver(this);

    dialog_ = new AppInfoDialog(
        widget_->GetNativeWindow(), extension_environment_.profile(),
        extension_environment_.MakePackagedApp(kTestExtensionId, true).get());
    widget_->GetContentsView()->AddChildView(dialog_);
  }

  void TearDown() override {
    if (!widget_destroyed_)
      widget_->CloseNow();
    EXPECT_TRUE(widget_destroyed_);
    ash::test::AshTestBase::TearDown();
  }

 protected:
  // Overridden from views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override {
    widget_destroyed_ = true;
    widget_->RemoveObserver(this);
    widget_ = NULL;
  }

  void UninstallApp(const std::string& app_id) {
    extensions::ExtensionSystem::Get(extension_environment_.profile())
        ->extension_service()
        ->UninstallExtension(
            app_id, extensions::UninstallReason::UNINSTALL_REASON_FOR_TESTING,
            base::Closure(), NULL);
  }

 protected:
  views::Widget* widget_ = nullptr;
  bool widget_destroyed_ = false;
  AppInfoDialog* dialog_ = nullptr;  // Owned by |widget_|'s views hierarchy.
  extensions::TestExtensionEnvironment extension_environment_;

  DISALLOW_COPY_AND_ASSIGN(AppInfoDialogViewsTest);
};

// Tests that the dialog closes when the current app is uninstalled.
TEST_F(AppInfoDialogViewsTest, UninstallingAppClosesDialog) {
  EXPECT_FALSE(widget_->IsClosed());
  EXPECT_FALSE(widget_destroyed_);
  UninstallApp(kTestExtensionId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(widget_destroyed_);
}

// Tests that the dialog does not close when a different app is uninstalled.
TEST_F(AppInfoDialogViewsTest, UninstallingOtherAppDoesNotCloseDialog) {
  extension_environment_.MakePackagedApp(kTestOtherExtensionId, true);

  EXPECT_FALSE(widget_->IsClosed());
  EXPECT_FALSE(widget_destroyed_);
  UninstallApp(kTestOtherExtensionId);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(widget_destroyed_);
}

// Tests that the dialog closes when the current profile is destroyed.
TEST_F(AppInfoDialogViewsTest, DestroyedProfileClosesDialog) {
  EXPECT_FALSE(widget_->IsClosed());
  EXPECT_FALSE(widget_destroyed_);
  extension_environment_.DeleteProfile();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(widget_destroyed_);
}

// Tests that the dialog does not close when a different profile is destroyed.
TEST_F(AppInfoDialogViewsTest, DestroyedOtherProfileDoesNotCloseDialog) {
  scoped_ptr<TestingProfile> other_profile(new TestingProfile);
  extension_environment_.CreateExtensionServiceForProfile(other_profile.get());

  scoped_refptr<const extensions::Extension> other_app =
      extension_environment_.MakePackagedApp(kTestOtherExtensionId, false);
  extensions::ExtensionSystem::Get(other_profile.get())
      ->extension_service()
      ->AddExtension(other_app.get());

  EXPECT_FALSE(widget_->IsClosed());
  EXPECT_FALSE(widget_destroyed_);
  other_profile.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(widget_destroyed_);
}

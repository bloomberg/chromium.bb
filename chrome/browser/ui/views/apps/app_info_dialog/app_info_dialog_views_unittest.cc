// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_dialog_views.h"

#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_footer_panel.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

namespace {

const char kTestExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestOtherExtensionId[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

}  // namespace

class AppInfoDialogViewsTest : public ash::test::AshTestBase,
                               public views::WidgetObserver {
 public:
  AppInfoDialogViewsTest()
      : widget_(NULL),
        widget_destroyed_(false),
        command_line_(base::CommandLine::NO_PROGRAM) {}
  ~AppInfoDialogViewsTest() override {}

  // Overridden from testing::Test:
  void SetUp() override {
    ash::test::AshTestBase::SetUp();

    widget_ = views::DialogDelegate::CreateDialogWidget(
        new views::DialogDelegateView(), CurrentContext(), NULL);
    widget_->AddObserver(this);

    profile_.reset(new TestingProfile());
    CreateExtensionSystemForProfile(profile_.get());

    app_ = extensions::ExtensionBuilder()
               .SetManifest(ValidAppManifest())
               .SetID(kTestExtensionId)
               .Build();
    InstallApp(profile_.get(), app_.get());

    dialog_ = new AppInfoDialog(
        widget_->GetNativeWindow(), profile_.get(), app_.get());
    widget_->GetContentsView()->AddChildView(dialog_);
  }

  void TearDown() override {
    if (!widget_destroyed_)
      widget_->CloseNow();
    widget_ = NULL;
    profile_.reset();
    ash::test::AshTestBase::TearDown();
  }

 protected:
  // Overridden from views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override {
    widget_destroyed_ = true;
    widget_->RemoveObserver(this);
    widget_ = NULL;
  }

  void CreateExtensionSystemForProfile(Profile* profile) {
    extensions::TestExtensionSystem* test_extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile));
    test_extension_system->CreateExtensionService(
        &command_line_,
        base::FilePath() /* install_directory */,
        false /* autoupdate_enabled*/);
  }

  void InstallApp(Profile* profile, const extensions::Extension* app) {
    extensions::ExtensionSystem::Get(profile)
        ->extension_service()
        ->AddExtension(app);
  }

  void UninstallApp(const std::string& app_id) {
    extensions::ExtensionSystem::Get(profile_.get())
        ->extension_service()
        ->UninstallExtension(
            app_id,
            extensions::UninstallReason::UNINSTALL_REASON_FOR_TESTING,
            base::Closure(),
            NULL);
  }

  // TODO(sashab): Move this into extension_test_util.h and update the calls
  // from AppInfoPermissionsPanelTest as well.
  scoped_ptr<base::DictionaryValue> ValidAppManifest() {
    return extensions::DictionaryBuilder()
        .Set("name", "Test App Name")
        .Set("version", "2.0")
        .Set("manifest_version", 2)
        .Set("app",
             extensions::DictionaryBuilder().Set(
                 "background",
                 extensions::DictionaryBuilder().Set(
                     "scripts",
                     extensions::ListBuilder().Append("background.js"))))
        .Build();
  }

 protected:
  scoped_ptr<TestingProfile> profile_;
  views::Widget* widget_;
  bool widget_destroyed_;
  AppInfoDialog* dialog_;  // Owned by widget_ through views heirarchy.

 private:
  base::CommandLine command_line_;

  scoped_refptr<const extensions::Extension> app_;

#if defined OS_CHROMEOS
  // Set up a user manager so these tests will run on ChromeOS. These member
  // variables need to be initialized in this order.
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif

  DISALLOW_COPY_AND_ASSIGN(AppInfoDialogViewsTest);
};

// Tests that the dialog closes when the current app is uninstalled.
TEST_F(AppInfoDialogViewsTest, UninstallingAppClosesDialog) {
  EXPECT_FALSE(widget_->IsClosed());
  EXPECT_FALSE(widget_destroyed_);
  UninstallApp(kTestExtensionId);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(widget_destroyed_);
}

// Tests that the dialog does not close when a different app is uninstalled.
TEST_F(AppInfoDialogViewsTest, UninstallingOtherAppDoesNotCloseDialog) {
  scoped_refptr<const extensions::Extension> other_app =
      extensions::ExtensionBuilder()
          .SetManifest(ValidAppManifest())
          .SetID(kTestOtherExtensionId)
          .Build();
  extensions::ExtensionSystem::Get(profile_.get())
      ->extension_service()
      ->AddExtension(other_app.get());

  EXPECT_FALSE(widget_->IsClosed());
  EXPECT_FALSE(widget_destroyed_);
  UninstallApp(kTestOtherExtensionId);
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(widget_destroyed_);
}

// Tests that the dialog closes when the current profile is destroyed.
TEST_F(AppInfoDialogViewsTest, DestroyedProfileClosesDialog) {
  EXPECT_FALSE(widget_->IsClosed());
  EXPECT_FALSE(widget_destroyed_);
  profile_.reset();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(widget_destroyed_);
}

// Tests that the dialog does not close when a different profile is destroyed.
TEST_F(AppInfoDialogViewsTest, DestroyedOtherProfileDoesNotCloseDialog) {
  scoped_ptr<TestingProfile> other_profile;
  other_profile.reset(new TestingProfile());
  CreateExtensionSystemForProfile(other_profile.get());
  scoped_refptr<const extensions::Extension> other_app =
      extensions::ExtensionBuilder()
          .SetManifest(ValidAppManifest())
          .SetID(kTestOtherExtensionId)
          .Build();
  InstallApp(other_profile.get(), other_app.get());

  EXPECT_FALSE(widget_->IsClosed());
  EXPECT_FALSE(widget_destroyed_);
  other_profile.reset();
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(widget_destroyed_);
}

// Tests that the pin/unpin button is focused after unpinning/pinning. This is
// to verify regression in crbug.com/428704 is fixed.
TEST_F(AppInfoDialogViewsTest, PinButtonsAreFocusedAfterPinUnpin) {
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

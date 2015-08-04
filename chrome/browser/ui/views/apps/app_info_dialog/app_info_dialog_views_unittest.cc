// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_dialog_views.h"

#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_header_panel.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/extension_system.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/link.h"
#include "ui/views/test/scoped_views_test_helper.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

namespace test {

class AppInfoDialogTestApi {
 public:
  explicit AppInfoDialogTestApi(AppInfoDialog* dialog) : dialog_(dialog) {}

  AppInfoHeaderPanel* header_panel() {
    return static_cast<AppInfoHeaderPanel*>(dialog_->child_at(0));
  }

  views::Link* view_in_store_link() {
    return header_panel()->view_in_store_link_;
  }

 private:
  AppInfoDialog* dialog_;

  DISALLOW_COPY_AND_ASSIGN(AppInfoDialogTestApi);
};

}  // namespace test

namespace {

const char kTestExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestOtherExtensionId[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

}  // namespace

class AppInfoDialogViewsTest : public BrowserWithTestWindowTest,
                               public views::WidgetObserver {
 public:
  AppInfoDialogViewsTest()
      : extension_environment_(base::MessageLoopForUI::current()) {}

  // Overridden from testing::Test:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    widget_ = views::DialogDelegate::CreateDialogWidget(
        new views::DialogDelegateView(), GetContext(), nullptr);
    widget_->AddObserver(this);

    extension_ = extension_environment_.MakePackagedApp(kTestExtensionId, true);
    dialog_ = new AppInfoDialog(widget_->GetNativeWindow(),
                                extension_environment_.profile(),
                                extension_.get());

    widget_->GetContentsView()->AddChildView(dialog_);
    widget_->Show();
  }

  void TearDown() override {
    if (!widget_destroyed_)
      widget_->CloseNow();
    EXPECT_TRUE(widget_destroyed_);
    extension_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
  }

  // BrowserWithTestWindowTest:
  TestingProfile* CreateProfile() override {
    return extension_environment_.profile();
  }

  void DestroyProfile(TestingProfile* profile) override {}

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
  scoped_refptr<extensions::Extension> extension_;
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
  // First delete the test browser window. This ensures the test harness isn't
  // surprised by it being closed in response to the profile deletion below.
  // Note the base class doesn't own the profile, so that part is skipped.
  DestroyBrowserAndProfile();

  // The following does nothing: it just ensures the Widget close is being
  // triggered by the DeleteProfile() call rather than the line above.
  base::RunLoop().RunUntilIdle();

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

// Tests that clicking the View in Store link opens a browser tab and closes the
// dialog cleanly.
TEST_F(AppInfoDialogViewsTest, ViewInStore) {
  EXPECT_TRUE(extension_->from_webstore());  // Otherwise there is no link.
  views::Link* link = test::AppInfoDialogTestApi(dialog_).view_in_store_link();
  EXPECT_TRUE(link);

  TabStripModel* tabs = browser()->tab_strip_model();
  EXPECT_EQ(0, tabs->count());

  EXPECT_FALSE(widget_->IsClosed());
  EXPECT_FALSE(widget_destroyed_);
  link->OnKeyPressed(ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, 0));

  EXPECT_TRUE(widget_->IsClosed());
  EXPECT_FALSE(widget_destroyed_);

  EXPECT_EQ(1, tabs->count());
  content::WebContents* web_contents = tabs->GetWebContentsAt(0);

  std::string url = "https://chrome.google.com/webstore/detail/";
  url += kTestExtensionId;
  url += "?utm_source=chrome-app-launcher-info-dialog";
  EXPECT_EQ(GURL(url), web_contents->GetURL());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(widget_destroyed_);
}

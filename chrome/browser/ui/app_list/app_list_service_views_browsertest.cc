// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_service_views.h"

#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "ash/shell.h"
#include "ash/test/app_list_controller_test_api.h"
#endif

namespace {

app_list::AppListView* GetAppListView(AppListService* service) {
#if defined(OS_CHROMEOS)
  return ash::test::AppListControllerTestApi(ash::Shell::GetInstance()).view();
#else
  return static_cast<AppListServiceViews*>(service)->shower().app_list();
#endif
}

}  // namespace

namespace test {

// Allow access to private variables of the AppListView for testing.
class AppListViewTestApi {
 public:
  explicit AppListViewTestApi(app_list::AppListView* view) : view_(view) {}
  virtual ~AppListViewTestApi() {}

  bool is_overlay_visible() {
    DCHECK(view_->overlay_view_);
    return view_->overlay_view_->visible();
  }

 private:
  app_list::AppListView* view_;

  DISALLOW_COPY_AND_ASSIGN(AppListViewTestApi);
};

}  // namespace test

// Browser Test for AppListService on Views platforms.
typedef InProcessBrowserTest AppListServiceViewsBrowserTest;

// Test closing the native app list window as if via a request from the OS.
IN_PROC_BROWSER_TEST_F(AppListServiceViewsBrowserTest, NativeClose) {
  AppListService* service = AppListService::Get();
  EXPECT_FALSE(service->GetAppListWindow());

  // Since the profile is loaded, this will create a view immediately. This is
  // important, because anything asynchronous would need an interactive_uitest
  // due to the possibility of the app list being dismissed, and
  // AppListService::GetAppListWindow returning NULL.
  service->ShowForProfile(browser()->profile());
  gfx::NativeWindow window = service->GetAppListWindow();
  EXPECT_TRUE(window);

  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  EXPECT_TRUE(widget);
  widget->Close();

  // Close is asynchronous (dismiss is not) so sink the message queue.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(service->GetAppListWindow());

  // Show again to get some code coverage for possibly stale pointers.
  service->ShowForProfile(browser()->profile());
  EXPECT_TRUE(service->GetAppListWindow());
  service->DismissAppList();  // Note: in Ash, this will invalidate the window.

  // Note: no need to sink message queue.
  EXPECT_FALSE(service->GetAppListWindow());
}

// Dismiss the app list via an accelerator when it is the only thing keeping
// Chrome alive and expect everything to clean up properly. This is a regression
// test for http://crbug.com/395937.
// Flaky on Win and Linux. https://crbug.com/477697
#if defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_AcceleratorClose DISABLED_AcceleratorClose
#else
#define MAYBE_AcceleratorClose AcceleratorClose
#endif
IN_PROC_BROWSER_TEST_F(AppListServiceViewsBrowserTest, MAYBE_AcceleratorClose) {
  AppListService* service = AppListService::Get();
  service->ShowForProfile(browser()->profile());
  EXPECT_TRUE(service->GetAppListWindow());

  content::WindowedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, content::Source<Browser>(browser()));
  chrome::CloseWindow(browser());
  close_observer.Wait();

  ui::test::EventGenerator generator(service->GetAppListWindow());
  generator.PressKey(ui::VKEY_ESCAPE, 0);

#if !defined(OS_CHROMEOS)
  EXPECT_TRUE(chrome::WillKeepAlive());
#endif

  base::RunLoop().RunUntilIdle();

#if !defined(OS_CHROMEOS)
  EXPECT_FALSE(chrome::WillKeepAlive());
#endif
  EXPECT_FALSE(service->GetAppListWindow());
}

// Tests for opening the app info dialog from the app list.
class AppListControllerAppInfoDialogBrowserTest : public ExtensionBrowserTest {
 public:
  AppListControllerAppInfoDialogBrowserTest() {}
  ~AppListControllerAppInfoDialogBrowserTest() override {}

 protected:
  // content::BrowserTestBase:
  void SetUpOnMainThread() override {
    // Install a test extension.
    base::FilePath test_extension_path;
    EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_extension_path));
    test_extension_path = test_extension_path.AppendASCII("extensions")
                              .AppendASCII("platform_apps")
                              .AppendASCII("minimal");
    extension_ = InstallExtension(
        test_extension_path, 1 /* expected_change: new install */);
    EXPECT_TRUE(extension_);

    // Open the app list.
    service_ = AppListService::Get();
    EXPECT_FALSE(service_->GetAppListWindow());
    service_->ShowForProfile(browser()->profile());
    app_list_view_ = GetAppListView(service_);
    EXPECT_TRUE(app_list_view_);
    native_view_ = app_list_view_->GetWidget()->GetNativeView();
    EXPECT_TRUE(native_view_);
  }

  void OpenAppInfoDialog() {
    AppListControllerDelegate* controller = service_->GetControllerDelegate();
    EXPECT_TRUE(controller);
    EXPECT_TRUE(controller->GetAppListWindow());
    controller->DoShowAppInfoFlow(browser()->profile(), extension_->id());
  }

  AppListService* service_;
  const extensions::Extension* extension_;
  app_list::AppListView* app_list_view_;
  gfx::NativeView native_view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListControllerAppInfoDialogBrowserTest);
};

// Test the DoShowAppInfoFlow function of the controller delegate.
// flaky: http://crbug.com/378251
IN_PROC_BROWSER_TEST_F(AppListControllerAppInfoDialogBrowserTest,
                       DISABLED_DoShowAppInfoFlow) {
  test::AppListViewTestApi test_api(app_list_view_);

  views::Widget::Widgets owned_widgets;
  views::Widget::GetAllOwnedWidgets(native_view_, &owned_widgets);
  EXPECT_EQ(0U, owned_widgets.size());
  EXPECT_FALSE(test_api.is_overlay_visible());

  OpenAppInfoDialog();

  owned_widgets.clear();
  views::Widget::GetAllOwnedWidgets(native_view_, &owned_widgets);
  EXPECT_EQ(1U, owned_widgets.size());
  EXPECT_TRUE(test_api.is_overlay_visible());

  // Close the app info dialog.
  views::Widget* app_info_dialog = *owned_widgets.begin();
  app_info_dialog->CloseNow();

  owned_widgets.clear();
  views::Widget::GetAllOwnedWidgets(native_view_, &owned_widgets);
  EXPECT_EQ(0U, owned_widgets.size());
  EXPECT_FALSE(test_api.is_overlay_visible());
}

// Check that the app list can be closed with the app info dialog
// open without crashing. This is a regression test for http://crbug.com/443066.
IN_PROC_BROWSER_TEST_F(AppListControllerAppInfoDialogBrowserTest,
                       CanCloseAppListWithAppInfoOpen) {
  OpenAppInfoDialog();

  // Close the app list window.
  app_list_view_->GetWidget()->CloseNow();
  EXPECT_FALSE(GetAppListView(service_));
}

using AppListServiceViewsExtensionBrowserTest = ExtensionBrowserTest;

IN_PROC_BROWSER_TEST_F(AppListServiceViewsExtensionBrowserTest,
                       ShowForAppInstall) {
  // Install an extension to open the dialog for.
  base::FilePath test_extension_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_extension_path));
  test_extension_path = test_extension_path.AppendASCII("extensions")
                            .AppendASCII("platform_apps")
                            .AppendASCII("minimal");
  const extensions::Extension* extension = InstallExtension(
      test_extension_path, 1 /* expected_change: new install */);
  ASSERT_TRUE(extension);

  // Open the app list window for the app.
  AppListService* service = AppListService::Get();
  EXPECT_FALSE(service->GetAppListWindow());

  service->ShowForAppInstall(browser()->profile(), extension->id(), false);
  app_list::AppListView* app_list_view = GetAppListView(service);
  ASSERT_TRUE(app_list_view);

  app_list::ContentsView* contents_view =
      app_list_view->app_list_main_view()->contents_view();

  EXPECT_TRUE(contents_view->IsStateActive(app_list::AppListModel::STATE_APPS));
}

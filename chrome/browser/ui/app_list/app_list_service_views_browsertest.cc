// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_service_views.h"

#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/views/app_list_view.h"
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
  AppListService* service = test::GetAppListService();
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

// Browser Test for AppListController that ensures the App Info dialog opens
// correctly.
class AppListControllerAppInfoDialogBrowserTest : public ExtensionBrowserTest {
 public:
  AppListControllerAppInfoDialogBrowserTest() {}
  virtual ~AppListControllerAppInfoDialogBrowserTest() {}

  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListControllerAppInfoDialogBrowserTest);
};

// Test the DoShowAppInfoFlow function of the controller delegate.
// flaky: http://crbug.com/378251
IN_PROC_BROWSER_TEST_F(AppListControllerAppInfoDialogBrowserTest,
                       DISABLED_DoShowAppInfoFlow) {
  // Install an extension to open the dialog for.
  base::FilePath test_extension_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_extension_path));
  test_extension_path = test_extension_path.AppendASCII("extensions")
                            .AppendASCII("platform_apps")
                            .AppendASCII("minimal");
  const extensions::Extension* extension = InstallExtension(
      test_extension_path, 1 /* expected_change: new install */);
  ASSERT_TRUE(extension);

  // Open the app list window.
  AppListService* service = test::GetAppListService();
  EXPECT_FALSE(service->GetAppListWindow());

  service->ShowForProfile(browser()->profile());
  gfx::NativeWindow window = service->GetAppListWindow();
  EXPECT_TRUE(window);

  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  ASSERT_TRUE(widget);

  test::AppListViewTestApi test_api(GetAppListView(service));

  // Open the app info dialog.
  views::Widget::Widgets owned_widgets;
  widget->GetAllOwnedWidgets(window, &owned_widgets);
  EXPECT_EQ(0U, owned_widgets.size());
  EXPECT_FALSE(test_api.is_overlay_visible());

  AppListControllerDelegate* controller = service->GetControllerDelegate();
  ASSERT_TRUE(controller);
  controller->DoShowAppInfoFlow(browser()->profile(), extension->id());

  owned_widgets.clear();
  widget->GetAllOwnedWidgets(window, &owned_widgets);
  EXPECT_EQ(1U, owned_widgets.size());
  EXPECT_TRUE(test_api.is_overlay_visible());

  // Close the app info dialog.
  views::Widget* app_info_dialog = *owned_widgets.begin();
  app_info_dialog->CloseNow();

  owned_widgets.clear();
  widget->GetAllOwnedWidgets(window, &owned_widgets);
  EXPECT_EQ(0U, owned_widgets.size());
  EXPECT_FALSE(test_api.is_overlay_visible());
}

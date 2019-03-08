// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/app_menu_button_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/extension_toolbar_menu_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using bookmarks::BookmarkModel;

class ToolbarViewInteractiveUITest : public AppMenuButtonObserver,
                                     public extensions::ExtensionBrowserTest {
 public:
  ToolbarViewInteractiveUITest() = default;
  ~ToolbarViewInteractiveUITest() override = default;

  // AppMenuButtonObserver:
  void AppMenuShown() override;

  void OnWidgetDragWillStart();
  void OnWidgetDragComplete();

  // Starts a drag to the app menu button.
  void StartDrag();

 protected:
  AppMenuButton* GetAppMenuButton() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetAppMenuButton();
  }
  BrowserActionsContainer* GetBrowserActions() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetBrowserActionsContainer();
  }
  void set_task_runner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    task_runner_ = task_runner;
  }
  void set_quit_closure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }
  bool menu_shown() const { return menu_shown_; }

 private:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  bool menu_shown_ = false;
  base::OnceClosure quit_closure_;
};

void ToolbarViewInteractiveUITest::AppMenuShown() {
  menu_shown_ = true;

  // Release the mouse button.
  ui_controls::SendMouseEventsNotifyWhenDone(
      ui_controls::LEFT, ui_controls::UP,
      base::BindOnce(&ToolbarViewInteractiveUITest::OnWidgetDragComplete,
                     base::Unretained(this)));
}

void ToolbarViewInteractiveUITest::OnWidgetDragWillStart() {
  // Enqueue an event to move the mouse to the app menu button, which should
  // result in calling AppMenuShown().
  const gfx::Point target =
      ui_test_utils::GetCenterInScreenCoordinates(GetAppMenuButton());
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(base::IgnoreResult(&ui_controls::SendMouseMove),
                                target.x(), target.y()));
}

void ToolbarViewInteractiveUITest::OnWidgetDragComplete() {
  // Return control to the testcase.
  std::move(quit_closure_).Run();
}

void ToolbarViewInteractiveUITest::StartDrag() {
  // Move the mouse outside the toolbar action.
  const views::View* toolbar_action =
      GetBrowserActions()->GetToolbarActionViewAt(0);
  gfx::Point target(toolbar_action->width() + 1, toolbar_action->height() / 2);
  views::View::ConvertPointToScreen(toolbar_action, &target);
  EXPECT_TRUE(ui_controls::SendMouseMove(target.x(), target.y()));

  OnWidgetDragWillStart();
}

void ToolbarViewInteractiveUITest::SetUpOnMainThread() {
  extensions::ExtensionBrowserTest::SetUpOnMainThread();

  BrowserAppMenuButton::g_open_app_immediately_for_testing = true;
  ExtensionToolbarMenuView::set_close_menu_delay_for_testing(base::TimeDelta());
  ToolbarActionsBar::disable_animations_for_testing_ = true;
}

#if defined(OS_LINUX) && defined(USE_AURA)
// TODO(pkasting): https://crbug.com/923188 Flaky
#define MAYBE_TestAppMenuOpensOnDrag DISABLED_TestAppMenuOpensOnDrag
#elif defined(OS_MACOSX)
// TODO(pkasting): https://crbug.com/910435 Test hangs in the run loop on Mac, I
// don't know why.
#define MAYBE_TestAppMenuOpensOnDrag DISABLED_TestAppMenuOpensOnDrag
#elif defined(USE_OZONE)
// TODO(pkasting): https://crbug.com/910423 Can't post mouse events from
// background threads on Ozone, which is required to avoid hanging.
#define MAYBE_TestAppMenuOpensOnDrag DISABLED_TestAppMenuOpensOnDrag
#else
#define MAYBE_TestAppMenuOpensOnDrag TestAppMenuOpensOnDrag
#endif
IN_PROC_BROWSER_TEST_F(ToolbarViewInteractiveUITest,
                       MAYBE_TestAppMenuOpensOnDrag) {
  // Load an extension that has a browser action.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("basics")));
  // Ensure the extension is fully loaded, and that the next steps will happen
  // with a clean slate.
  base::RunLoop().RunUntilIdle();

  // Set up observers that will drive the test along.
  AppMenuButton* const app_menu_button = GetAppMenuButton();
  EXPECT_FALSE(app_menu_button->IsMenuShowing());
  ScopedObserver<AppMenuButton, AppMenuButtonObserver> button_observer(this);
  button_observer.Add(app_menu_button);

  // Set up the task runner to use for posting drag actions.
  // TODO(devlin): This is basically ViewEventTestBase::GetDragTaskRunner().  In
  // a perfect world, this would be factored better.
  // Drag events must be posted from a background thread, since starting a drag
  // triggers a nested message loop that filters messages other than mouse
  // events, so further tasks on the main message loop will be blocked.
  base::ScopedAllowBaseSyncPrimitivesForTesting allow_thread_join;
  base::Thread drag_event_thread("drag-event-thread");
  drag_event_thread.Start();
  set_task_runner(drag_event_thread.task_runner());

  // Click on the toolbar action.
  BrowserActionsContainer* const browser_actions = GetBrowserActions();
  ASSERT_EQ(1u, browser_actions->VisibleBrowserActions());
  ToolbarActionView* toolbar_action =
      browser_actions->GetToolbarActionViewAt(0);
  ASSERT_TRUE(toolbar_action);
  ui_test_utils::MoveMouseToCenterAndPress(
      toolbar_action, ui_controls::LEFT, ui_controls::DOWN,
      base::BindRepeating(&ToolbarViewInteractiveUITest::StartDrag,
                          base::Unretained(this)));
  base::RunLoop run_loop;
  set_quit_closure(run_loop.QuitWhenIdleClosure());
  run_loop.Run();

  // Verify postconditions.
  EXPECT_TRUE(menu_shown());
  // The app menu should have closed once the drag-and-drop completed.
  EXPECT_FALSE(app_menu_button->IsMenuShowing());
}

class ToolbarViewTest : public InProcessBrowserTest {
 public:
  ToolbarViewTest() {}

  void RunToolbarCycleFocusTest(Browser* browser);

 private:
  DISALLOW_COPY_AND_ASSIGN(ToolbarViewTest);
};

void ToolbarViewTest::RunToolbarCycleFocusTest(Browser* browser) {
  gfx::NativeWindow window = browser->window()->GetNativeWindow();
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);

  // Test relies on browser window activation, while platform such as Linux's
  // window activation is asynchronous.
  views::test::WidgetActivationWaiter waiter(widget, true);
  waiter.Wait();

  // Send focus to the toolbar as if the user pressed Alt+Shift+T. This should
  // happen after the browser window activation.
  CommandUpdater* updater = browser->command_controller();
  updater->ExecuteCommand(IDC_FOCUS_TOOLBAR);

  views::FocusManager* focus_manager = widget->GetFocusManager();
  views::View* first_view = focus_manager->GetFocusedView();
  std::vector<int> ids;

  // Press Tab to cycle through all of the controls in the toolbar until
  // we end up back where we started.
  bool found_reload = false;
  bool found_location_bar = false;
  bool found_app_menu = false;
  const views::View* view = NULL;
  while (view != first_view) {
    focus_manager->AdvanceFocus(false);
    view = focus_manager->GetFocusedView();
    ids.push_back(view->id());
    if (view->id() == VIEW_ID_RELOAD_BUTTON)
      found_reload = true;
    if (view->id() == VIEW_ID_APP_MENU)
      found_app_menu = true;
    if (view->id() == VIEW_ID_OMNIBOX)
      found_location_bar = true;
    if (ids.size() > 100)
      GTEST_FAIL() << "Tabbed 100 times, still haven't cycled back!";
  }

  // Make sure we found a few key items.
  ASSERT_TRUE(found_reload);
  ASSERT_TRUE(found_app_menu);
  ASSERT_TRUE(found_location_bar);

  // Now press Shift-Tab to cycle backwards.
  std::vector<int> reverse_ids;
  view = NULL;
  while (view != first_view) {
    focus_manager->AdvanceFocus(true);
    view = focus_manager->GetFocusedView();
    reverse_ids.push_back(view->id());
    if (reverse_ids.size() > 100)
      GTEST_FAIL() << "Tabbed 100 times, still haven't cycled back!";
  }

  // Assert that the views were focused in exactly the reverse order.
  // The sequences should be the same length, and the last element will
  // be the same, and the others are reverse.
  ASSERT_EQ(ids.size(), reverse_ids.size());
  size_t count = ids.size();
  for (size_t i = 0; i < count - 1; i++)
    EXPECT_EQ(ids[i], reverse_ids[count - 2 - i]);
}

#if defined(OS_MACOSX)
// Widget activation doesn't work on Mac: https://crbug.com/823543
#define MAYBE_ToolbarCycleFocus DISABLED_ToolbarCycleFocus
#else
#define MAYBE_ToolbarCycleFocus ToolbarCycleFocus
#endif
IN_PROC_BROWSER_TEST_F(ToolbarViewTest, MAYBE_ToolbarCycleFocus) {
  RunToolbarCycleFocusTest(browser());
}

#if defined(OS_MACOSX)
// Widget activation doesn't work on Mac: https://crbug.com/823543
#define MAYBE_ToolbarCycleFocusWithBookmarkBar \
  DISABLED_ToolbarCycleFocusWithBookmarkBar
#else
#define MAYBE_ToolbarCycleFocusWithBookmarkBar ToolbarCycleFocusWithBookmarkBar
#endif
IN_PROC_BROWSER_TEST_F(ToolbarViewTest,
                       MAYBE_ToolbarCycleFocusWithBookmarkBar) {
  CommandUpdater* updater = browser()->command_controller();
  updater->ExecuteCommand(IDC_SHOW_BOOKMARK_BAR);

  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::AddIfNotBookmarked(model, GURL("http://foo.com"),
                                base::ASCIIToUTF16("Foo"));

  // We want to specifically test the case where the bookmark bar is
  // already showing when a window opens, so create a second browser
  // window with the same profile.
  Browser* second_browser = CreateBrowser(browser()->profile());
  RunToolbarCycleFocusTest(second_browser);
}

IN_PROC_BROWSER_TEST_F(ToolbarViewTest, BackButtonUpdate) {
  ToolbarView* toolbar =
      BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
  EXPECT_FALSE(toolbar->back_button()->enabled());

  // Navigate to title1.html. Back button should be enabled.
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("title1.html")));
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(toolbar->back_button()->enabled());

  // Delete old navigations. Back button will be disabled.
  auto& controller =
      browser()->tab_strip_model()->GetActiveWebContents()->GetController();
  controller.DeleteNavigationEntries(base::BindRepeating(
      [&](content::NavigationEntry* entry) { return true; }));
  EXPECT_FALSE(toolbar->back_button()->enabled());
}

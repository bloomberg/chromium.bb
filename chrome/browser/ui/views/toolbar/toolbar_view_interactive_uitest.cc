// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_drag_controller_interactive_uitest.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/wrench_toolbar_button.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "extensions/common/feature_switch.h"

// Borrowed from chrome/browser/ui/views/bookmarks/bookmark_bar_view_test.cc,
// since these are also disabled on Linux for drag and drop.
// TODO(erg): Fix DND tests on linux_aura. crbug.com/163931
#if defined(OS_LINUX) && defined(USE_AURA)
#define MAYBE(x) DISABLED_##x
#else
#define MAYBE(x) x
#endif

class ToolbarViewInteractiveUITest : public ExtensionBrowserTest {
 public:
  ToolbarViewInteractiveUITest();
  virtual ~ToolbarViewInteractiveUITest();

 protected:
  ToolbarView* toolbar_view() { return toolbar_view_; }
  BrowserActionsContainer* browser_actions() { return browser_actions_; }

  // Performs a drag-and-drop operation by moving the mouse to |start|, clicking
  // the left button, moving the mouse to |end|, and releasing the left button.
  // TestWhileInDragOperation() is called after the mouse has moved to |end|,
  // but before the click is released.
  void DoDragAndDrop(const gfx::Point& start, const gfx::Point& end);

  // A function to perform testing actions while in the drag operation from
  // DoDragAndDrop.
  void TestWhileInDragOperation();

 private:
  // Finishes the drag-and-drop operation started in DoDragAndDrop().
  void FinishDragAndDrop(const base::Closure& quit_closure);

  // InProcessBrowserTest:
  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE;
  virtual void SetUpOnMainThread() OVERRIDE;

  ToolbarView* toolbar_view_;

  BrowserActionsContainer* browser_actions_;

  // The drag-and-drop background thread.
  scoped_ptr<base::Thread> dnd_thread_;

  // Override the extensions-action-redesign switch.
  scoped_ptr<extensions::FeatureSwitch::ScopedOverride> feature_override_;
};

ToolbarViewInteractiveUITest::ToolbarViewInteractiveUITest()
    : toolbar_view_(NULL),
      browser_actions_(NULL) {
}

ToolbarViewInteractiveUITest::~ToolbarViewInteractiveUITest() {
}

void ToolbarViewInteractiveUITest::DoDragAndDrop(const gfx::Point& start,
                                                 const gfx::Point& end) {
  // Much of this function is modeled after methods in ViewEventTestBase (in
  // particular, the |dnd_thread_|, but it's easier to move that here than try
  // to make ViewEventTestBase play nice with a BrowserView (for the toolbar).
  // TODO(devlin): In a perfect world, this would be factored better.

  // Send the mouse to |start|, and click.
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(start));
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
                  ui_controls::LEFT, ui_controls::DOWN));

  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner();

  ui_controls::SendMouseMoveNotifyWhenDone(
      end.x() + 10,
      end.y(),
      base::Bind(&ToolbarViewInteractiveUITest::FinishDragAndDrop,
                 base::Unretained(this),
                 runner->QuitClosure()));

  // Also post a move task to the drag and drop thread.
  if (!dnd_thread_.get()) {
    dnd_thread_.reset(new base::Thread("mouse_move_thread"));
    dnd_thread_->Start();
  }

  dnd_thread_->message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&ui_controls::SendMouseMove),
                 end.x(),
                 end.y()),
      base::TimeDelta::FromMilliseconds(200));
  runner->Run();
}

void ToolbarViewInteractiveUITest::TestWhileInDragOperation() {
  EXPECT_TRUE(toolbar_view()->IsWrenchMenuShowing());
}

void ToolbarViewInteractiveUITest::FinishDragAndDrop(
    const base::Closure& quit_closure) {
  dnd_thread_.reset();
  TestWhileInDragOperation();
  ui_controls::SendMouseEvents(ui_controls::LEFT, ui_controls::UP);
  ui_controls::RunClosureAfterAllPendingUIEvents(
      quit_closure);
}

void ToolbarViewInteractiveUITest::SetUpCommandLine(
    base::CommandLine* command_line) {
  ExtensionBrowserTest::SetUpCommandLine(command_line);
  // We do this before the rest of the setup because it can affect how the views
  // are constructed.
  feature_override_.reset(new extensions::FeatureSwitch::ScopedOverride(
      extensions::FeatureSwitch::extension_action_redesign(), true));
}

void ToolbarViewInteractiveUITest::SetUpOnMainThread() {
  ExtensionBrowserTest::SetUpOnMainThread();

  toolbar_view_ = BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
  browser_actions_ = toolbar_view_->browser_actions();
}

IN_PROC_BROWSER_TEST_F(ToolbarViewInteractiveUITest,
                       MAYBE(TestWrenchMenuOpensOnDrag)) {
  BrowserActionsContainer::disable_animations_during_testing_ = true;
  WrenchToolbarButton::g_open_wrench_immediately_for_testing = true;

  // Load an extension that has a browser action.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("basics")));
  base::RunLoop().RunUntilIdle();  // Ensure the extension is fully loaded.

  ASSERT_EQ(1u, browser_actions()->VisibleBrowserActions());

  BrowserActionView* view = browser_actions()->GetBrowserActionViewAt(0);
  ASSERT_TRUE(view);

  gfx::Point browser_action_view_loc = test::GetCenterInScreenCoordinates(view);
  gfx::Point wrench_button_loc =
      test::GetCenterInScreenCoordinates(toolbar_view()->app_menu());

  // Perform a drag and drop from the browser action view to the wrench button,
  // which should open the wrench menu.
  DoDragAndDrop(browser_action_view_loc, wrench_button_loc);
}

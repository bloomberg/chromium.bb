// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_test_helpers.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/view_event_test_base.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/test_browser_thread.h"
#include "grit/generated_resources.h"
#include "ui/base/accessibility/accessibility_types.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/widget/widget.h"

using content::BrowserThread;
using content::OpenURLParams;
using content::PageNavigator;
using content::WebContents;

namespace {

void MoveMouseAndPress(const gfx::Point& screen_pos,
                       ui_controls::MouseButton button,
                       int state,
                       const base::Closure& closure) {
  ui_controls::SendMouseMove(screen_pos.x(), screen_pos.y());
  ui_controls::SendMouseEventsNotifyWhenDone(button, state, closure);
}

// PageNavigator implementation that records the URL.
class TestingPageNavigator : public PageNavigator {
 public:
  virtual WebContents* OpenURL(const OpenURLParams& params) OVERRIDE {
    url_ = params.url;
    return NULL;
  }

  GURL url_;
};

// TODO(jschuh): Fix bookmark DND tests on Win64. crbug.com/244605
#if defined(OS_WIN) && defined(ARCH_CPU_X86_64)
#define MAYBE(x) DISABLED_##x
#else
#define MAYBE(x) x
#endif

}  // namespace

// Base class for event generating bookmark view tests. These test are intended
// to exercise View's menus, but that's easier done with BookmarkBarView rather
// than View's menu itself.
//
// SetUp creates a bookmark model with the following structure.
// All folders are in upper case, all URLs in lower case.
// F1
//   f1a
//   F11
//     f11a
//   *
// a
// b
// c
// d
// F2
// e
// OTHER
//   oa
//   OF
//     ofa
//     ofb
//   OF2
//     of2a
//     of2b
//
// * if CreateBigMenu returns return true, 100 menu items are created here with
//   the names f1-f100.
//
// Subclasses should be sure and invoke super's implementation of SetUp and
// TearDown.
class BookmarkBarViewEventTestBase : public ViewEventTestBase {
 public:
  BookmarkBarViewEventTestBase()
      : ViewEventTestBase(),
        model_(NULL) {}

  virtual void SetUp() OVERRIDE {
    views::MenuController::TurnOffMenuSelectionHoldForTest();
    BookmarkBarView::DisableAnimationsForTesting(true);

    profile_.reset(new TestingProfile());
    profile_->CreateBookmarkModel(true);
    model_ = BookmarkModelFactory::GetForProfile(profile_.get());
    test::WaitForBookmarkModelToLoad(model_);
    profile_->GetPrefs()->SetBoolean(prefs::kShowBookmarkBar, true);

    Browser::CreateParams native_params(profile_.get(),
                                        chrome::GetActiveDesktop());
    browser_.reset(
        chrome::CreateBrowserWithTestWindowForParams(&native_params));

    local_state_.reset(new ScopedTestingLocalState(
        TestingBrowserProcess::GetGlobal()));
    model_->ClearStore();

    bb_view_.reset(new BookmarkBarView(browser_.get(), NULL));
    bb_view_->set_owned_by_client();
    bb_view_->SetPageNavigator(&navigator_);

    AddTestData(CreateBigMenu());

    // Calculate the preferred size so that one button doesn't fit, which
    // triggers the overflow button to appear.
    //
    // BookmarkBarView::Layout does nothing if the parent is NULL and
    // GetPreferredSize hard codes a width of 1. For that reason we add the
    // BookmarkBarView to a dumby view as the parent.
    //
    // This code looks a bit hacky, but I've written it so that it shouldn't
    // be dependant upon any of the layout code in BookmarkBarView. Instead
    // we brute force search for a size that triggers the overflow button.
    views::View tmp_parent;

    tmp_parent.AddChildView(bb_view_.get());

    bb_view_pref_ = bb_view_->GetPreferredSize();
    bb_view_pref_.set_width(1000);
    views::TextButton* button = GetBookmarkButton(6);
    while (button->visible()) {
      bb_view_pref_.set_width(bb_view_pref_.width() - 25);
      bb_view_->SetBounds(0, 0, bb_view_pref_.width(), bb_view_pref_.height());
      bb_view_->Layout();
    }

    tmp_parent.RemoveChildView(bb_view_.get());

    ViewEventTestBase::SetUp();
  }

  virtual void TearDown() {
    // Destroy everything, then run the message loop to ensure we delete all
    // Tasks and fully shut down.
    browser_->tab_strip_model()->CloseAllTabs();
    bb_view_.reset();
    browser_.reset();
    profile_.reset();

    // Run the message loop to ensure we delete allTasks and fully shut down.
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::MessageLoop::QuitClosure());
    base::MessageLoop::current()->Run();

    ViewEventTestBase::TearDown();
    BookmarkBarView::DisableAnimationsForTesting(false);
  }

 protected:
  virtual views::View* CreateContentsView() OVERRIDE {
    return bb_view_.get();
  }

  virtual gfx::Size GetPreferredSize() OVERRIDE { return bb_view_pref_; }

  views::TextButton* GetBookmarkButton(int view_index) {
    return bb_view_->GetBookmarkButton(view_index);
  }

  // See comment above class description for what this does.
  virtual bool CreateBigMenu() { return false; }

  BookmarkModel* model_;
  scoped_ptr<BookmarkBarView> bb_view_;
  TestingPageNavigator navigator_;

 private:
  void AddTestData(bool big_menu) {
    const BookmarkNode* bb_node = model_->bookmark_bar_node();
    std::string test_base = "file:///c:/tmp/";
    const BookmarkNode* f1 = model_->AddFolder(bb_node, 0, ASCIIToUTF16("F1"));
    model_->AddURL(f1, 0, ASCIIToUTF16("f1a"), GURL(test_base + "f1a"));
    const BookmarkNode* f11 = model_->AddFolder(f1, 1, ASCIIToUTF16("F11"));
    model_->AddURL(f11, 0, ASCIIToUTF16("f11a"), GURL(test_base + "f11a"));
    if (big_menu) {
      for (int i = 1; i <= 100; ++i) {
        model_->AddURL(f1, i + 1, ASCIIToUTF16("f") + base::IntToString16(i),
                       GURL(test_base + "f" + base::IntToString(i)));
      }
    }
    model_->AddURL(bb_node, 1, ASCIIToUTF16("a"), GURL(test_base + "a"));
    model_->AddURL(bb_node, 2, ASCIIToUTF16("b"), GURL(test_base + "b"));
    model_->AddURL(bb_node, 3, ASCIIToUTF16("c"), GURL(test_base + "c"));
    model_->AddURL(bb_node, 4, ASCIIToUTF16("d"), GURL(test_base + "d"));
    model_->AddFolder(bb_node, 5, ASCIIToUTF16("F2"));
    model_->AddURL(bb_node, 6, ASCIIToUTF16("d"), GURL(test_base + "d"));

    model_->AddURL(model_->other_node(), 0, ASCIIToUTF16("oa"),
                   GURL(test_base + "oa"));
    const BookmarkNode* of = model_->AddFolder(model_->other_node(), 1,
                                               ASCIIToUTF16("OF"));
    model_->AddURL(of, 0, ASCIIToUTF16("ofa"), GURL(test_base + "ofa"));
    model_->AddURL(of, 1, ASCIIToUTF16("ofb"), GURL(test_base + "ofb"));
    const BookmarkNode* of2 = model_->AddFolder(model_->other_node(), 2,
                                                ASCIIToUTF16("OF2"));
    model_->AddURL(of2, 0, ASCIIToUTF16("of2a"), GURL(test_base + "of2a"));
    model_->AddURL(of2, 1, ASCIIToUTF16("of2b"), GURL(test_base + "of2b"));
  }

  gfx::Size bb_view_pref_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<Browser> browser_;
  scoped_ptr<ScopedTestingLocalState> local_state_;
};

// Clicks on first menu, makes sure button is depressed. Moves mouse to first
// child, clicks it and makes sure a navigation occurs.
class BookmarkBarViewTest1 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() OVERRIDE {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest1::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Button should be depressed.
    views::TextButton* button = GetBookmarkButton(0);
    ASSERT_TRUE(button->state() == views::CustomButton::STATE_PRESSED);

    // Click on the 2nd menu item (A URL).
    ASSERT_TRUE(menu->GetSubmenu());

    views::MenuItemView* menu_to_select =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ui_test_utils::MoveMouseToCenterAndPress(menu_to_select, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest1::Step3));
  }

  void Step3() {
    // We should have navigated to URL f1a.
    ASSERT_TRUE(navigator_.url_ ==
                model_->bookmark_bar_node()->GetChild(0)->GetChild(0)->url());

    // Make sure button is no longer pushed.
    views::TextButton* button = GetBookmarkButton(0);
    ASSERT_TRUE(button->state() == views::CustomButton::STATE_NORMAL);

    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu == NULL || !menu->GetSubmenu()->IsShowing());

    Done();
  }
};

VIEW_TEST(BookmarkBarViewTest1, Basic)

// Brings up menu, clicks on empty space and make sure menu hides.
class BookmarkBarViewTest2 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() OVERRIDE {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest2::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL && menu->GetSubmenu()->IsShowing());

    // Click on 0x0, which should trigger closing menu.
    // NOTE: this code assume there is a left margin, which is currently
    // true. If that changes, this code will need to find another empty space
    // to press the mouse on.
    gfx::Point mouse_loc;
    views::View::ConvertPointToScreen(bb_view_.get(), &mouse_loc);
    ui_controls::SendMouseMoveNotifyWhenDone(0, 0,
        CreateEventTask(this, &BookmarkBarViewTest2::Step3));
  }

  void Step3() {
    // As the click is on the desktop the hook never sees the up, so we only
    // wait on the down. We still send the up though else the system thinks
    // the mouse is still down.
    ui_controls::SendMouseEventsNotifyWhenDone(
        ui_controls::LEFT, ui_controls::DOWN,
        CreateEventTask(this, &BookmarkBarViewTest2::Step4));
    ui_controls::SendMouseEvents(ui_controls::LEFT, ui_controls::UP);
  }

  void Step4() {
    // The menu shouldn't be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu == NULL || !menu->GetSubmenu()->IsShowing());

    // Make sure button is no longer pushed.
    views::TextButton* button = GetBookmarkButton(0);
    ASSERT_TRUE(button->state() == views::CustomButton::STATE_NORMAL);

    Done();
  }
};

VIEW_TEST(BookmarkBarViewTest2, HideOnDesktopClick)

// Brings up menu. Moves over child to make sure submenu appears, moves over
// another child and make sure next menu appears.
class BookmarkBarViewTest3 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() OVERRIDE {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::MenuButton* button = bb_view_->other_bookmarked_button();
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest3::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(child_menu != NULL);

    // Click on second child, which has a submenu.
    ui_test_utils::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest3::Step3));
  }

  void Step3() {
    // Make sure sub menu is showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu);
    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(child_menu->GetSubmenu() != NULL);
    ASSERT_TRUE(child_menu->GetSubmenu()->IsShowing());

    // Click on third child, which has a submenu too.
    child_menu = menu->GetSubmenu()->GetMenuItemAt(2);
    ASSERT_TRUE(child_menu != NULL);
    ui_test_utils::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest3::Step4));
  }

  void Step4() {
    // Make sure sub menu we first clicked isn't showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(child_menu->GetSubmenu() != NULL);
    ASSERT_FALSE(child_menu->GetSubmenu()->IsShowing());

    // And submenu we last clicked is showing.
    child_menu = menu->GetSubmenu()->GetMenuItemAt(2);
    ASSERT_TRUE(child_menu != NULL);
    ASSERT_TRUE(child_menu->GetSubmenu()->IsShowing());

    // Nothing should have been selected.
    EXPECT_EQ(GURL(), navigator_.url_);

    // Hide menu.
    menu->GetMenuController()->CancelAll();

    Done();
  }
};

VIEW_TEST(BookmarkBarViewTest3, Submenus)

// Observer that posts task upon the context menu creation.
// This is necessary for Linux as the context menu has to check
// the clipboard, which invokes the event loop.
class ContextMenuNotificationObserver : public content::NotificationObserver {
 public:
  explicit ContextMenuNotificationObserver(const base::Closure& task)
      : task_(task) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_BOOKMARK_CONTEXT_MENU_SHOWN,
                   content::NotificationService::AllSources());
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    base::MessageLoop::current()->PostTask(FROM_HERE, task_);
  }

  // Sets the task that is posted when the context menu is shown.
  void set_task(const base::Closure& task) { task_ = task; }

 private:
  content::NotificationRegistrar registrar_;
  base::Closure task_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuNotificationObserver);
};

// Tests context menus by way of opening a context menu for a bookmark,
// then right clicking to get context menu and selecting the first menu item
// (open).
class BookmarkBarViewTest4 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest4()
      : observer_(CreateEventTask(this, &BookmarkBarViewTest4::Step3)) {
  }

 protected:
  virtual void DoTestOnMessageLoop() OVERRIDE {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = bb_view_->other_bookmarked_button();
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest4::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ASSERT_TRUE(child_menu != NULL);

    // Right click on the first child to get its context menu.
    ui_test_utils::MoveMouseToCenterAndPress(child_menu, ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP, base::Closure());
    // Step3 will be invoked by ContextMenuNotificationObserver.
  }

  void Step3() {
    // Make sure the context menu is showing.
    views::MenuItemView* menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu());
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Select the first menu item (open).
    ui_test_utils::MoveMouseToCenterAndPress(
        menu->GetSubmenu()->GetMenuItemAt(0),
        ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest4::Step4));
  }

  void Step4() {
    EXPECT_EQ(navigator_.url_, model_->other_node()->GetChild(0)->url());
    Done();
  }

  ContextMenuNotificationObserver observer_;
};

VIEW_TEST(BookmarkBarViewTest4, ContextMenus)

// Tests drag and drop within the same menu.
class BookmarkBarViewTest5 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() OVERRIDE {
    url_dragging_ =
        model_->bookmark_bar_node()->GetChild(0)->GetChild(0)->url();

    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest5::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ASSERT_TRUE(child_menu != NULL);

    // Move mouse to center of menu and press button.
    ui_test_utils::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
        ui_controls::DOWN,
        CreateEventTask(this, &BookmarkBarViewTest5::Step3));
  }

  void Step3() {
    views::MenuItemView* target_menu =
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(1);
    gfx::Point loc(1, target_menu->height() - 1);
    views::View::ConvertPointToScreen(target_menu, &loc);

    // Start a drag.
    ui_controls::SendMouseMoveNotifyWhenDone(loc.x() + 10, loc.y(),
        CreateEventTask(this, &BookmarkBarViewTest5::Step4));

    // See comment above this method as to why we do this.
    ScheduleMouseMoveInBackground(loc.x(), loc.y());
  }

  void Step4() {
    // Drop the item so that it's now the second item.
    views::MenuItemView* target_menu =
        bb_view_->GetMenu()->GetSubmenu()->GetMenuItemAt(1);
    gfx::Point loc(1, target_menu->height() - 2);
    views::View::ConvertPointToScreen(target_menu, &loc);
    ui_controls::SendMouseMove(loc.x(), loc.y());

    ui_controls::SendMouseEventsNotifyWhenDone(ui_controls::LEFT,
        ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest5::Step5));
  }

  void Step5() {
    GURL url = model_->bookmark_bar_node()->GetChild(0)->GetChild(1)->url();
    EXPECT_EQ(url_dragging_, url);
    Done();
  }

  GURL url_dragging_;
};

VIEW_TEST(BookmarkBarViewTest5, MAYBE(DND))

// Tests holding mouse down on overflow button, dragging such that menu pops up
// then selecting an item.
class BookmarkBarViewTest6 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() OVERRIDE {
    // Press the mouse button on the overflow button. Don't release it though.
    views::TextButton* button = bb_view_->overflow_button();
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN, CreateEventTask(this, &BookmarkBarViewTest6::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ASSERT_TRUE(child_menu != NULL);

    // Move mouse to center of menu and release mouse.
    ui_test_utils::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
        ui_controls::UP, CreateEventTask(this, &BookmarkBarViewTest6::Step3));
  }

  void Step3() {
    ASSERT_TRUE(navigator_.url_ ==
                model_->bookmark_bar_node()->GetChild(6)->url());
    Done();
  }

  GURL url_dragging_;
};

VIEW_TEST(BookmarkBarViewTest6, OpenMenuOnClickAndHold)

// Tests drag and drop to different menu.
class BookmarkBarViewTest7 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() OVERRIDE {
    url_dragging_ =
        model_->bookmark_bar_node()->GetChild(0)->GetChild(0)->url();

    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest7::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ASSERT_TRUE(child_menu != NULL);

    // Move mouse to center of menu and press button.
    ui_test_utils::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
        ui_controls::DOWN,
        CreateEventTask(this, &BookmarkBarViewTest7::Step3));
  }

  void Step3() {
    // Drag over other button.
    views::TextButton* other_button =
        bb_view_->other_bookmarked_button();
    gfx::Point loc(other_button->width() / 2, other_button->height() / 2);
    views::View::ConvertPointToScreen(other_button, &loc);

#if defined(USE_AURA)
    // TODO: fix this. Aura requires an additional mouse event to trigger drag
    // and drop checking state.
    ui_controls::SendMouseMoveNotifyWhenDone(loc.x() + 10, loc.y(),
        base::Bind(&BookmarkBarViewTest7::Step3A, this));
#else
    // Start a drag.
    ui_controls::SendMouseMoveNotifyWhenDone(loc.x() + 10, loc.y(),
        base::Bind(&BookmarkBarViewTest7::Step4, this));

    // See comment above this method as to why we do this.
    ScheduleMouseMoveInBackground(loc.x(), loc.y());
#endif
  }

  void Step3A() {
    // Drag over other button.
    views::TextButton* other_button =
        bb_view_->other_bookmarked_button();
    gfx::Point loc(other_button->width() / 2, other_button->height() / 2);
    views::View::ConvertPointToScreen(other_button, &loc);

    ui_controls::SendMouseMoveNotifyWhenDone(loc.x(), loc.y(),
        base::Bind(&BookmarkBarViewTest7::Step4, this));
  }

  void Step4() {
    views::MenuItemView* drop_menu = bb_view_->GetDropMenu();
    ASSERT_TRUE(drop_menu != NULL);
    ASSERT_TRUE(drop_menu->GetSubmenu()->IsShowing());

    views::MenuItemView* target_menu =
        drop_menu->GetSubmenu()->GetMenuItemAt(0);
    gfx::Point loc(1, 1);
    views::View::ConvertPointToScreen(target_menu, &loc);
    ui_controls::SendMouseMoveNotifyWhenDone(loc.x(), loc.y(),
        CreateEventTask(this, &BookmarkBarViewTest7::Step5));
  }

  void Step5() {
    ui_controls::SendMouseEventsNotifyWhenDone(
        ui_controls::LEFT, ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest7::Step6));
  }

  void Step6() {
    ASSERT_TRUE(model_->other_node()->GetChild(0)->url() == url_dragging_);
    Done();
  }

  GURL url_dragging_;
};

#if !(defined(OS_WIN) && defined(USE_AURA))
// This test passes locally (on aero and non-aero) but fails on the trybots and
// buildbot.
// http://crbug.com/154081
VIEW_TEST(BookmarkBarViewTest7, MAYBE(DNDToDifferentMenu))
#endif

// Drags from one menu to next so that original menu closes, then back to
// original menu.
class BookmarkBarViewTest8 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() OVERRIDE {
    url_dragging_ =
        model_->bookmark_bar_node()->GetChild(0)->GetChild(0)->url();

    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest8::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ASSERT_TRUE(child_menu != NULL);

    // Move mouse to center of menu and press button.
    ui_test_utils::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
        ui_controls::DOWN,
        CreateEventTask(this, &BookmarkBarViewTest8::Step3));
  }

  void Step3() {
    // Drag over other button.
    views::TextButton* other_button =
        bb_view_->other_bookmarked_button();
    gfx::Point loc(other_button->width() / 2, other_button->height() / 2);
    views::View::ConvertPointToScreen(other_button, &loc);

    // Start a drag.
#if defined(USE_AURA)
    // TODO: fix this. Aura requires an additional mouse event to trigger drag
    // and drop checking state.
    ui_controls::SendMouseMoveNotifyWhenDone(loc.x() + 10, loc.y(),
        base::Bind(&BookmarkBarViewTest8::Step3A, this));
#else
    ui_controls::SendMouseMoveNotifyWhenDone(loc.x() + 10, loc.y(),
        base::Bind(&BookmarkBarViewTest8::Step4, this));
    // See comment above this method as to why we do this.
    ScheduleMouseMoveInBackground(loc.x(), loc.y());
#endif
  }

  void Step3A() {
    // Drag over other button.
    views::TextButton* other_button =
        bb_view_->other_bookmarked_button();
    gfx::Point loc(other_button->width() / 2, other_button->height() / 2);
    views::View::ConvertPointToScreen(other_button, &loc);

    ui_controls::SendMouseMoveNotifyWhenDone(loc.x() + 10, loc.y(),
        base::Bind(&BookmarkBarViewTest8::Step4, this));
  }

  void Step4() {
    views::MenuItemView* drop_menu = bb_view_->GetDropMenu();
    ASSERT_TRUE(drop_menu != NULL);
    ASSERT_TRUE(drop_menu->GetSubmenu()->IsShowing());

    // Now drag back over first menu.
    views::TextButton* button = GetBookmarkButton(0);
    gfx::Point loc(button->width() / 2, button->height() / 2);
    views::View::ConvertPointToScreen(button, &loc);
    ui_controls::SendMouseMoveNotifyWhenDone(loc.x(), loc.y(),
        base::Bind(&BookmarkBarViewTest8::Step5, this));
  }

  void Step5() {
    // Drop on folder F11.
    views::MenuItemView* drop_menu = bb_view_->GetDropMenu();
    ASSERT_TRUE(drop_menu != NULL);
    ASSERT_TRUE(drop_menu->GetSubmenu()->IsShowing());

    views::MenuItemView* target_menu =
        drop_menu->GetSubmenu()->GetMenuItemAt(1);
    ui_test_utils::MoveMouseToCenterAndPress(
        target_menu, ui_controls::LEFT, ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest8::Step6));
  }

  void Step6() {
    // Make sure drop was processed.
    GURL final_url = model_->bookmark_bar_node()->GetChild(0)->GetChild(0)->
        GetChild(1)->url();
    ASSERT_TRUE(final_url == url_dragging_);
    Done();
  }

  GURL url_dragging_;
};

#if !(defined(OS_WIN) && defined(USE_AURA))
// This test passes locally (on aero and non-aero) but fails on the trybots and
// buildbot.
// http://crbug.com/154081
VIEW_TEST(BookmarkBarViewTest8, MAYBE(DNDBackToOriginatingMenu))
#endif

// Moves the mouse over the scroll button and makes sure we get scrolling.
class BookmarkBarViewTest9 : public BookmarkBarViewEventTestBase {
 protected:
  virtual bool CreateBigMenu() OVERRIDE { return true; }

  virtual void DoTestOnMessageLoop() OVERRIDE {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest9::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    first_menu_ = menu->GetSubmenu()->GetMenuItemAt(0);
    gfx::Point menu_loc;
    views::View::ConvertPointToScreen(first_menu_, &menu_loc);
    start_y_ = menu_loc.y();

    // Move the mouse over the scroll button.
    views::View* scroll_container = menu->GetSubmenu()->parent();
    ASSERT_TRUE(scroll_container != NULL);
    scroll_container = scroll_container->parent();
    ASSERT_TRUE(scroll_container != NULL);
    views::View* scroll_down_button = scroll_container->child_at(1);
    ASSERT_TRUE(scroll_down_button);
    gfx::Point loc(scroll_down_button->width() / 2,
                   scroll_down_button->height() / 2);
    views::View::ConvertPointToScreen(scroll_down_button, &loc);

    // On linux, the sending one location isn't enough.
    ui_controls::SendMouseMove(loc.x() - 1 , loc.y() - 1);
    ui_controls::SendMouseMoveNotifyWhenDone(
        loc.x(), loc.y(), CreateEventTask(this, &BookmarkBarViewTest9::Step3));
  }

  void Step3() {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&BookmarkBarViewTest9::Step4, this),
        base::TimeDelta::FromMilliseconds(200));
  }

  void Step4() {
    gfx::Point menu_loc;
    views::View::ConvertPointToScreen(first_menu_, &menu_loc);
    ASSERT_NE(start_y_, menu_loc.y());

    // Hide menu.
    bb_view_->GetMenu()->GetMenuController()->CancelAll();

    // On linux, Cancelling menu will call Quit on the message loop,
    // which can interfere with Done. We need to run Done in the
    // next execution loop.
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&ViewEventTestBase::Done, this));
  }

  int start_y_;
  views::MenuItemView* first_menu_;
};

VIEW_TEST(BookmarkBarViewTest9, ScrollButtonScrolls)

// Tests up/down/left/enter key messages.
class BookmarkBarViewTest10 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() OVERRIDE {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest10::Step2));
    base::MessageLoop::current()->RunUntilIdle();
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Send a down event, which should select the first item.
    ui_controls::SendKeyPressNotifyWhenDone(
        window_->GetNativeWindow(), ui::VKEY_DOWN, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest10::Step3));
  }

  void Step3() {
    // Make sure menu is showing and item is selected.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());
    ASSERT_TRUE(menu->GetSubmenu()->GetMenuItemAt(0)->IsSelected());

    // Send a key down event, which should select the next item.
    ui_controls::SendKeyPressNotifyWhenDone(
        window_->GetNativeWindow(), ui::VKEY_DOWN, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest10::Step4));
  }

  void Step4() {
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());
    ASSERT_FALSE(menu->GetSubmenu()->GetMenuItemAt(0)->IsSelected());
    ASSERT_TRUE(menu->GetSubmenu()->GetMenuItemAt(1)->IsSelected());

    // Send a right arrow to force the menu to open.
    ui_controls::SendKeyPressNotifyWhenDone(
        window_->GetNativeWindow(), ui::VKEY_RIGHT, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest10::Step5));
  }

  void Step5() {
    // Make sure the submenu is showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());
    views::MenuItemView* submenu = menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(submenu->IsSelected());
    ASSERT_TRUE(submenu->GetSubmenu());
    ASSERT_TRUE(submenu->GetSubmenu()->IsShowing());

    // Send a left arrow to close the submenu.
    ui_controls::SendKeyPressNotifyWhenDone(
        window_->GetNativeWindow(), ui::VKEY_LEFT, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest10::Step6));
  }

  void Step6() {
    // Make sure the submenu is showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());
    views::MenuItemView* submenu = menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(submenu->IsSelected());
    ASSERT_TRUE(!submenu->GetSubmenu() || !submenu->GetSubmenu()->IsShowing());

    // Send a down arrow to wrap back to f1a
    ui_controls::SendKeyPressNotifyWhenDone(
        window_->GetNativeWindow(), ui::VKEY_DOWN, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest10::Step7));
  }

  void Step7() {
    // Make sure menu is showing and item is selected.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());
    ASSERT_TRUE(menu->GetSubmenu()->GetMenuItemAt(0)->IsSelected());

    // Send enter, which should select the item.
    ui_controls::SendKeyPressNotifyWhenDone(
        window_->GetNativeWindow(), ui::VKEY_RETURN, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest10::Step8));
  }

  void Step8() {
    ASSERT_TRUE(
        model_->bookmark_bar_node()->GetChild(0)->GetChild(0)->url() ==
        navigator_.url_);
    Done();
  }
};

VIEW_TEST(BookmarkBarViewTest10, DISABLED_KeyEvents)

// Make sure the menu closes with the following sequence: show menu, show
// context menu, close context menu (via escape), then click else where. This
// effectively verifies we maintain mouse capture after the context menu is
// hidden.
class BookmarkBarViewTest11 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest11()
      : observer_(CreateEventTask(this, &BookmarkBarViewTest11::Step3)) {
  }

 protected:
  virtual void DoTestOnMessageLoop() OVERRIDE {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = bb_view_->other_bookmarked_button();
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest11::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ASSERT_TRUE(child_menu != NULL);

    // Right click on the first child to get its context menu.
    ui_test_utils::MoveMouseToCenterAndPress(child_menu, ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP, base::Closure());
    // Step3 will be invoked by ContextMenuNotificationObserver.
  }

  void Step3() {
    // Send escape so that the context menu hides.
    ui_controls::SendKeyPressNotifyWhenDone(
        window_->GetNativeWindow(), ui::VKEY_ESCAPE, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest11::Step4));
  }

  void Step4() {
    // Make sure the context menu is no longer showing.
    views::MenuItemView* menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(!menu || !menu->GetSubmenu() ||
                !menu->GetSubmenu()->IsShowing());

    // But the menu should be showing.
    menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu && menu->GetSubmenu() && menu->GetSubmenu()->IsShowing());

    // Now click on empty space.
    gfx::Point mouse_loc;
    views::View::ConvertPointToScreen(bb_view_.get(), &mouse_loc);
    ui_controls::SendMouseMove(mouse_loc.x(), mouse_loc.y());
    ui_controls::SendMouseEventsNotifyWhenDone(
        ui_controls::LEFT, ui_controls::UP | ui_controls::DOWN,
        CreateEventTask(this, &BookmarkBarViewTest11::Step5));
  }

  void Step5() {
    // Make sure the menu is not showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(!menu || !menu->GetSubmenu() ||
                !menu->GetSubmenu()->IsShowing());
    Done();
  }

  ContextMenuNotificationObserver observer_;
};

VIEW_TEST(BookmarkBarViewTest11, CloseMenuAfterClosingContextMenu)

// Tests showing a modal dialog from a context menu.
class BookmarkBarViewTest12 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() OVERRIDE {
    // Open up the other folder.
    views::TextButton* button = bb_view_->other_bookmarked_button();
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest12::Step2));
    chrome::num_bookmark_urls_before_prompting = 1;
  }

  virtual ~BookmarkBarViewTest12() {
    chrome::num_bookmark_urls_before_prompting = 15;
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(child_menu != NULL);

    // Right click on the second child (a folder) to get its context menu.
    ui_test_utils::MoveMouseToCenterAndPress(child_menu, ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest12::Step3));
  }

  void Step3() {
    // Make sure the context menu is showing.
    views::MenuItemView* menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(menu && menu->GetSubmenu() && menu->GetSubmenu()->IsShowing());

    // Select the first item in the context menu (open all).
    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ASSERT_TRUE(child_menu != NULL);
    ui_test_utils::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP, base::Closure());

    // Delay until we send tab, otherwise the message box doesn't appear
    // correctly.
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        CreateEventTask(this, &BookmarkBarViewTest12::Step4),
        base::TimeDelta::FromSeconds(1));
  }

  void Step4() {
    // Press tab to give focus to the cancel button.
    ui_controls::SendKeyPress(
        window_->GetNativeWindow(), ui::VKEY_TAB, false, false, false, false);

    // For some reason return isn't processed correctly unless we delay.
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        CreateEventTask(this, &BookmarkBarViewTest12::Step5),
        base::TimeDelta::FromSeconds(1));
  }

  void Step5() {
    // And press enter so that the cancel button is selected.
    ui_controls::SendKeyPressNotifyWhenDone(
        window_->GetNativeWindow(), ui::VKEY_RETURN, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest12::Step6));
  }

  void Step6() {
    // Do a delayed task to give the dialog time to exit.
    base::MessageLoop::current()->PostTask(
        FROM_HERE, CreateEventTask(this, &BookmarkBarViewTest12::Step7));
  }

  void Step7() {
    Done();
  }
};

VIEW_TEST(BookmarkBarViewTest12, CloseWithModalDialog)

// Tests clicking on the separator of a context menu (this is for coverage of
// bug 17862).
class BookmarkBarViewTest13 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest13()
      : observer_(CreateEventTask(this, &BookmarkBarViewTest13::Step3)) {
  }

 protected:
  virtual void DoTestOnMessageLoop() OVERRIDE {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = bb_view_->other_bookmarked_button();
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest13::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ASSERT_TRUE(child_menu != NULL);

    // Right click on the first child to get its context menu.
    ui_test_utils::MoveMouseToCenterAndPress(child_menu, ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP, base::Closure());
    // Step3 will be invoked by ContextMenuNotificationObserver.
  }

  void Step3() {
    // Make sure the context menu is showing.
    views::MenuItemView* menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu());
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Find the first separator.
    views::SubmenuView* submenu = menu->GetSubmenu();
    views::View* separator_view = NULL;
    for (int i = 0; i < submenu->child_count(); ++i) {
      if (submenu->child_at(i)->id() != views::MenuItemView::kMenuItemViewID) {
        separator_view = submenu->child_at(i);
        break;
      }
    }
    ASSERT_TRUE(separator_view);

    // Click on the separator. Clicking on the separator shouldn't visually
    // change anything.
    ui_test_utils::MoveMouseToCenterAndPress(separator_view,
        ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest13::Step4));
  }

  void Step4() {
    // The context menu should still be showing.
    views::MenuItemView* menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu());
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Select the first context menu item.
    ui_test_utils::MoveMouseToCenterAndPress(
        menu->GetSubmenu()->GetMenuItemAt(0),
        ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest13::Step5));
  }

  void Step5() {
    Done();
  }

  ContextMenuNotificationObserver observer_;
};

VIEW_TEST(BookmarkBarViewTest13, ClickOnContextMenuSeparator)

// Makes sure right clicking on a folder on the bookmark bar doesn't result in
// both a context menu and showing the menu.
class BookmarkBarViewTest14 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest14()
      : observer_(CreateEventTask(this, &BookmarkBarViewTest14::Step2)) {
  }

 protected:
  virtual void DoTestOnMessageLoop() OVERRIDE {
    // Move the mouse to the first folder on the bookmark bar and press the
    // right mouse button.
    views::TextButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP, base::Closure());
    // Step2 will be invoked by ContextMenuNotificationObserver.
  }

 private:

  void Step2() {
    // Menu should NOT be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu == NULL);

    // Send escape so that the context menu hides.
    ui_controls::SendKeyPressNotifyWhenDone(
        window_->GetNativeWindow(), ui::VKEY_ESCAPE, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest14::Step3));
  }

  void Step3() {
    Done();
  }

  ContextMenuNotificationObserver observer_;
};

VIEW_TEST(BookmarkBarViewTest14, ContextMenus2)

// Makes sure deleting from the context menu keeps the bookmark menu showing.
class BookmarkBarViewTest15 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest15()
      : deleted_menu_id_(0),
        observer_(CreateEventTask(this, &BookmarkBarViewTest15::Step3)) {
  }

 protected:
  virtual void DoTestOnMessageLoop() OVERRIDE {
    // Show the other bookmarks.
    views::TextButton* button = bb_view_->other_bookmarked_button();
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest15::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(child_menu != NULL);

    deleted_menu_id_ = child_menu->GetCommand();

    // Right click on the second child to get its context menu.
    ui_test_utils::MoveMouseToCenterAndPress(child_menu, ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP, base::Closure());
    // Step3 will be invoked by ContextMenuNotificationObserver.
  }

  void Step3() {
    // Make sure the context menu is showing.
    views::MenuItemView* menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu());
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* delete_menu =
        menu->GetMenuItemByID(IDC_BOOKMARK_BAR_REMOVE);
    ASSERT_TRUE(delete_menu);

    // Click on the delete button.
    ui_test_utils::MoveMouseToCenterAndPress(delete_menu,
        ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest15::Step4));
  }

  void Step4() {
    // The context menu should not be showing.
    views::MenuItemView* context_menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(context_menu == NULL);

    // But the menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // And the deleted_menu_id_ should have been removed.
    ASSERT_TRUE(menu->GetMenuItemByID(deleted_menu_id_) == NULL);

    bb_view_->GetMenu()->GetMenuController()->CancelAll();

    Done();
  }

  int deleted_menu_id_;
  ContextMenuNotificationObserver observer_;
};

VIEW_TEST(BookmarkBarViewTest15, MenuStaysVisibleAfterDelete)

// Tests that we don't crash or get stuck if the parent of a menu is closed.
class BookmarkBarViewTest16 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() OVERRIDE {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest16::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Button should be depressed.
    views::TextButton* button = GetBookmarkButton(0);
    ASSERT_TRUE(button->state() == views::CustomButton::STATE_PRESSED);

    // Close the window.
    window_->Close();
    window_ = NULL;

    base::MessageLoop::current()->PostTask(
        FROM_HERE, CreateEventTask(this, &BookmarkBarViewTest16::Done));
  }
};

VIEW_TEST(BookmarkBarViewTest16, DeleteMenu)

// Makes sure right clicking on an item while a context menu is already showing
// doesn't crash and works.
class BookmarkBarViewTest17 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest17()
      : observer_(CreateEventTask(this, &BookmarkBarViewTest17::Step3)) {
  }

 protected:
  virtual void DoTestOnMessageLoop() OVERRIDE {
    // Move the mouse to the other folder on the bookmark bar and press the
    // left mouse button.
    views::TextButton* button = bb_view_->other_bookmarked_button();
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest17::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Right click on the second item to show its context menu.
    views::MenuItemView* child_menu = menu->GetSubmenu()->GetMenuItemAt(2);
    ASSERT_TRUE(child_menu != NULL);
    ui_test_utils::MoveMouseToCenterAndPress(child_menu, ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP, base::Closure());
    // Step3 will be invoked by ContextMenuNotificationObserver.
  }

  void Step3() {
    // Make sure the context menu is showing.
    views::MenuItemView* context_menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(context_menu != NULL);
    ASSERT_TRUE(context_menu->GetSubmenu());
    ASSERT_TRUE(context_menu->GetSubmenu()->IsShowing());

    // Right click on the first menu item to trigger its context menu.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());
    views::MenuItemView* child_menu = menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(child_menu != NULL);

    // The context menu and child_menu can be overlapped, calculate the
    // non-intersected Rect of the child menu and click on its center to make
    // sure the click is always on the child menu.
    gfx::Rect context_rect = context_menu->GetSubmenu()->GetBoundsInScreen();
    gfx::Rect child_menu_rect = child_menu->GetBoundsInScreen();
    gfx::Rect clickable_rect =
        gfx::SubtractRects(child_menu_rect, context_rect);
    ASSERT_FALSE(clickable_rect.IsEmpty());
    observer_.set_task(CreateEventTask(this, &BookmarkBarViewTest17::Step4));
    MoveMouseAndPress(clickable_rect.CenterPoint(), ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP, base::Closure());
    // Step4 will be invoked by ContextMenuNotificationObserver.
  }

  void Step4() {
    // The context menu should still be showing.
    views::MenuItemView* context_menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(context_menu != NULL);

    // And the menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    bb_view_->GetMenu()->GetMenuController()->CancelAll();

    Done();
  }

  ContextMenuNotificationObserver observer_;
};

VIEW_TEST(BookmarkBarViewTest17, ContextMenus3)

// Verifies sibling menus works. Clicks on the 'other bookmarks' folder, then
// moves the mouse over the first item on the bookmark bar and makes sure the
// menu appears.
class BookmarkBarViewTest18 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() OVERRIDE {
    // Move the mouse to the other folder on the bookmark bar and press the
    // left mouse button.
    views::TextButton* button = bb_view_->other_bookmarked_button();
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest18::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Move the mouse to the first folder on the bookmark bar
    views::TextButton* button = GetBookmarkButton(0);
    gfx::Point button_center(button->width() / 2, button->height() / 2);
    views::View::ConvertPointToScreen(button, &button_center);
    ui_controls::SendMouseMoveNotifyWhenDone(
        button_center.x(), button_center.y(),
        CreateEventTask(this, &BookmarkBarViewTest18::Step3));
  }

  void Step3() {
    // Make sure the menu is showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // The menu for the first folder should be in the pressed state (since the
    // menu is showing for it).
    EXPECT_EQ(views::CustomButton::STATE_PRESSED,
              GetBookmarkButton(0)->state());

    menu->GetMenuController()->CancelAll();

    Done();
  }
};

VIEW_TEST(BookmarkBarViewTest18, BookmarkBarViewTest18_SiblingMenu)

// Verifies mousing over an already open sibling menu doesn't prematurely cancel
// the menu.
class BookmarkBarViewTest19 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() OVERRIDE {
    // Move the mouse to the other folder on the bookmark bar and press the
    // left mouse button.
    views::TextButton* button = bb_view_->other_bookmarked_button();
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest19::Step2));
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Click on the first folder.
    views::MenuItemView* child_menu = menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(child_menu != NULL);
    ui_test_utils::MoveMouseToCenterAndPress(
        child_menu, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest19::Step3));
  }

  void Step3() {
    // Make sure the menu is showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Move the mouse back to the other bookmark button.
    views::TextButton* button = bb_view_->other_bookmarked_button();
    gfx::Point button_center(button->width() / 2, button->height() / 2);
    views::View::ConvertPointToScreen(button, &button_center);
    ui_controls::SendMouseMoveNotifyWhenDone(
        button_center.x() + 1, button_center.y() + 1,
        CreateEventTask(this, &BookmarkBarViewTest19::Step4));
  }

  void Step4() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Click on the first folder.
    views::MenuItemView* child_menu = menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(child_menu != NULL);
    ui_test_utils::MoveMouseToCenterAndPress(
        child_menu,
        ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest19::Step5));
  }

  void Step5() {
    // Make sure the menu is showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    menu->GetMenuController()->CancelAll();

    Done();
  }
};

VIEW_TEST(BookmarkBarViewTest19, BookmarkBarViewTest19_SiblingMenu)

// Verify that when clicking a mouse button outside a context menu,
// the context menu is dismissed *and* the underlying view receives
// the the mouse event (due to event reposting).
class BookmarkBarViewTest20 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest20() : test_view_(new TestViewForMenuExit) {}

 protected:
  virtual void DoTestOnMessageLoop() OVERRIDE {
    // Add |test_view_| next to |bb_view_|.
    views::View* parent = bb_view_->parent();
    views::View* container_view = new ContainerViewForMenuExit;
    container_view->AddChildView(bb_view_.get());
    container_view->AddChildView(test_view_);
    parent->AddChildView(container_view);
    parent->Layout();

    ASSERT_EQ(test_view_->press_count(), 0);

    // Move the mouse to the Test View and press the left mouse button.
    ui_test_utils::MoveMouseToCenterAndPress(
        test_view_, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest20::Step1));
  }

 private:
  void Step1() {
    ASSERT_EQ(test_view_->press_count(), 1);
    ASSERT_TRUE(bb_view_->GetMenu() == NULL);

    // Move the mouse to the first folder on the bookmark bar and press the
    // left mouse button.
    views::TextButton* button = GetBookmarkButton(0);
    ui_test_utils::MoveMouseToCenterAndPress(
        button, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest20::Step2));
  }

  void Step2() {
    ASSERT_EQ(test_view_->press_count(), 1);
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Move the mouse to the Test View and press the left mouse button.
    // The context menu will consume the event and exit. Thereafter,
    // the event is reposted and delivered to the Test View which
    // increases its press-count.
    ui_test_utils::MoveMouseToCenterAndPress(
        test_view_, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest20::Step3));
  }

  void Step3() {
    ASSERT_EQ(test_view_->press_count(), 2);
    ASSERT_TRUE(bb_view_->GetMenu() == NULL);
    Done();
  }

  class ContainerViewForMenuExit : public views::View {
   public:
    ContainerViewForMenuExit() {
    }

    virtual void Layout() OVERRIDE {
      DCHECK_EQ(2, child_count());
      views::View* bb_view = child_at(0);
      views::View* test_view = child_at(1);
      const int width = bb_view->width();
      const int height = bb_view->height();
      bb_view->SetBounds(0,0, width - 22, height);
      test_view->SetBounds(width - 20, 0, 20, height);
    }

   private:

    DISALLOW_COPY_AND_ASSIGN(ContainerViewForMenuExit);
  };

  class TestViewForMenuExit : public views::View {
   public:
    TestViewForMenuExit() : press_count_(0) {
    }
    virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE {
      ++press_count_;
      return true;
    }
    int press_count() const { return press_count_; }

   private:
    int press_count_;

    DISALLOW_COPY_AND_ASSIGN(TestViewForMenuExit);
  };

  TestViewForMenuExit* test_view_;
};

#if defined(OS_WIN) && !defined(USE_AURA)
#define MAYBE_ContextMenuExitTest DISABLED_ContextMenuExitTest
#else
#define MAYBE_ContextMenuExitTest ContextMenuExitTest
#endif

VIEW_TEST(BookmarkBarViewTest20, MAYBE_ContextMenuExitTest)

// Tests context menu by way of opening a context menu for a empty folder menu.
// The opened context menu should behave as it is from the folder button.
class BookmarkBarViewTest21 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest21()
      : observer_(CreateEventTask(this, &BookmarkBarViewTest21::Step3)) {
  }

 protected:
  // Move the mouse to the empty folder on the bookmark bar and press the
  // left mouse button.
  virtual void DoTestOnMessageLoop() OVERRIDE {
    views::TextButton* button = GetBookmarkButton(5);
    ui_test_utils::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest21::Step2));
  }

 private:
  // Confirm that a menu for empty folder shows and right click the menu.
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);

    views::SubmenuView* submenu = menu->GetSubmenu();
    ASSERT_TRUE(submenu->IsShowing());
    ASSERT_EQ(1, submenu->child_count());

    views::View* view = submenu->child_at(0);
    ASSERT_TRUE(view != NULL);
    EXPECT_EQ(views::MenuItemView::kEmptyMenuItemViewID, view->id());

    // Right click on the first child to get its context menu.
    ui_test_utils::MoveMouseToCenterAndPress(view, ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP, base::Closure());
    // Step3 will be invoked by ContextMenuNotificationObserver.
  }

  // Confirm that context menu shows and click REMOVE menu.
  void Step3() {
    // Make sure the context menu is showing.
    views::MenuItemView* menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu());
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    views::MenuItemView* delete_menu =
        menu->GetMenuItemByID(IDC_BOOKMARK_BAR_REMOVE);
    ASSERT_TRUE(delete_menu);

    // Click on the delete menu item.
    ui_test_utils::MoveMouseToCenterAndPress(delete_menu,
        ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest21::Step4));
  }

  // Confirm that the empty folder gets removed and menu doesn't show.
  void Step4() {
    views::TextButton* button = GetBookmarkButton(5);
    ASSERT_TRUE(button);
    EXPECT_EQ(ASCIIToUTF16("d"), button->text());
    EXPECT_TRUE(bb_view_->GetContextMenu() == NULL);
    EXPECT_TRUE(bb_view_->GetMenu() == NULL);

    Done();
  }

  ContextMenuNotificationObserver observer_;
};

VIEW_TEST(BookmarkBarViewTest21, ContextMenusForEmptyFolder)

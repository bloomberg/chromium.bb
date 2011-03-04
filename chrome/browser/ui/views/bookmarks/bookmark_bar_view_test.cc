// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/interactive_ui/view_event_test_base.h"
#include "chrome/test/testing_profile.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/tab_contents/page_navigator.h"
#include "grit/generated_resources.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/button/text_button.h"
#include "views/controls/menu/menu_controller.h"
#include "views/controls/menu/menu_item_view.h"
#include "views/controls/menu/submenu_view.h"
#include "views/views_delegate.h"
#include "views/window/window.h"

#if defined(OS_LINUX)
// See http://crbug.com/40040 for details.
#define MAYBE_DND DISABLED_DND
#define MAYBE_DNDToDifferentMenu DISABLED_DNDToDifferentMenu
#define MAYBE_DNDBackToOriginatingMenu DISABLED_DNDBackToOriginatingMenu

// See http://crbug.com/40039 for details.
#define MAYBE_KeyEvents DISABLED_KeyEvents

// Two bugs here. http://crbug.com/47089 for general Linux Views, and
// http://crbug.com/47452 for ChromiumOS.
#define MAYBE_CloseWithModalDialog DISABLED_CloseWithModalDialog
// See http://crbug.com/47089 for details.
#define MAYBE_CloseMenuAfterClosingContextMenu \
        DISABLED_CloseMenuAfterClosingContextMenu
#else

#define MAYBE_DND DND
#define MAYBE_DNDToDifferentMenu DNDToDifferentMenu
#define MAYBE_DNDBackToOriginatingMenu DNDBackToOriginatingMenu
#define MAYBE_DNDBackToOriginatingMenu DNDBackToOriginatingMenu
#define MAYBE_KeyEvents KeyEvents
#define MAYBE_CloseWithModalDialog CloseWithModalDialog
#define MAYBE_CloseMenuAfterClosingContextMenu CloseMenuAfterClosingContextMenu

#endif

#if defined(OS_LINUX)
// See bug http://crbug.com/60444 for details.
#define MAYBE_ScrollButtonScrolls DISABLED_ScrollButtonScrolls
#else
#define MAYBE_ScrollButtonScrolls ScrollButtonScrolls
#endif

namespace {

class ViewsDelegateImpl : public views::ViewsDelegate {
 public:
  ViewsDelegateImpl() {}
  virtual ui::Clipboard* GetClipboard() const { return NULL; }
  virtual void SaveWindowPlacement(views::Window* window,
                                   const std::wstring& window_name,
                                   const gfx::Rect& bounds,
                                   bool maximized) {}
  virtual bool GetSavedWindowBounds(views::Window* window,
                                    const std::wstring& window_name,
                                    gfx::Rect* bounds) const {
    return false;
  }
  virtual bool GetSavedMaximizedState(views::Window* window,
                                      const std::wstring& window_name,
                                      bool* maximized) const {
    return false;
  }

  virtual void NotifyAccessibilityEvent(
      views::View* view, AccessibilityTypes::Event event_type) {}

#if defined(OS_WIN)
  virtual HICON GetDefaultWindowIcon() const { return 0; }
#endif

  virtual void AddRef() {
  }

  virtual void ReleaseRef() {
    MessageLoopForUI::current()->Quit();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ViewsDelegateImpl);
};

// PageNavigator implementation that records the URL.
class TestingPageNavigator : public PageNavigator {
 public:
  virtual void OpenURL(const GURL& url, const GURL& referrer,
                       WindowOpenDisposition disposition,
                       PageTransition::Type transition) {
    url_ = url;
  }

  GURL url_;
};

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
        model_(NULL),
        bb_view_(NULL),
        ui_thread_(BrowserThread::UI, MessageLoop::current()),
        file_thread_(BrowserThread::FILE, MessageLoop::current()) {
  }

  virtual void SetUp() {
    BookmarkBarView::testing_ = true;

    profile_.reset(new TestingProfile());
    profile_->set_has_history_service(true);
    profile_->CreateBookmarkModel(true);
    profile_->BlockUntilBookmarkModelLoaded();
    profile_->GetPrefs()->SetBoolean(prefs::kShowBookmarkBar, true);

    model_ = profile_->GetBookmarkModel();
    model_->ClearStore();

    bb_view_ = new BookmarkBarView(profile_.get(), NULL);
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

    tmp_parent.AddChildView(bb_view_);

    bb_view_pref_ = bb_view_->GetPreferredSize();
    bb_view_pref_.set_width(1000);
    views::TextButton* button = bb_view_->GetBookmarkButton(4);
    while (button->IsVisible()) {
      bb_view_pref_.set_width(bb_view_pref_.width() - 25);
      bb_view_->SetBounds(0, 0, bb_view_pref_.width(), bb_view_pref_.height());
      bb_view_->Layout();
    }

    tmp_parent.RemoveChildView(bb_view_);

    ViewEventTestBase::SetUp();
  }

  virtual void TearDown() {
    ViewEventTestBase::TearDown();
    BookmarkBarView::testing_ = false;
    views::ViewsDelegate::views_delegate = NULL;
  }

 protected:
  void InstallViewsDelegate() {
    views::ViewsDelegate::views_delegate = &views_delegate_;
  }

  virtual views::View* CreateContentsView() {
    return bb_view_;
  }

  virtual gfx::Size GetPreferredSize() { return bb_view_pref_; }

  // See comment above class description for what this does.
  virtual bool CreateBigMenu() { return false; }

  BookmarkModel* model_;
  BookmarkBarView* bb_view_;
  TestingPageNavigator navigator_;

 private:
  void AddTestData(bool big_menu) {
    std::string test_base = "file:///c:/tmp/";

    const BookmarkNode* f1 = model_->AddGroup(
        model_->GetBookmarkBarNode(), 0, ASCIIToUTF16("F1"));
    model_->AddURL(f1, 0, ASCIIToUTF16("f1a"), GURL(test_base + "f1a"));
    const BookmarkNode* f11 = model_->AddGroup(f1, 1, ASCIIToUTF16("F11"));
    model_->AddURL(f11, 0, ASCIIToUTF16("f11a"), GURL(test_base + "f11a"));
    if (big_menu) {
      for (int i = 1; i <= 100; ++i) {
        model_->AddURL(f1, i + 1, ASCIIToUTF16("f") + base::IntToString16(i),
                       GURL(test_base + "f" + base::IntToString(i)));
      }
    }
    model_->AddURL(model_->GetBookmarkBarNode(), 1, ASCIIToUTF16("a"),
                   GURL(test_base + "a"));
    model_->AddURL(model_->GetBookmarkBarNode(), 2, ASCIIToUTF16("b"),
                   GURL(test_base + "b"));
    model_->AddURL(model_->GetBookmarkBarNode(), 3, ASCIIToUTF16("c"),
                   GURL(test_base + "c"));
    model_->AddURL(model_->GetBookmarkBarNode(), 4, ASCIIToUTF16("d"),
                   GURL(test_base + "d"));
    model_->AddURL(model_->other_node(), 0, ASCIIToUTF16("oa"),
                   GURL(test_base + "oa"));
    const BookmarkNode* of = model_->AddGroup(model_->other_node(), 1,
                                              ASCIIToUTF16("OF"));
    model_->AddURL(of, 0, ASCIIToUTF16("ofa"), GURL(test_base + "ofa"));
    model_->AddURL(of, 1, ASCIIToUTF16("ofb"), GURL(test_base + "ofb"));
    const BookmarkNode* of2 = model_->AddGroup(model_->other_node(), 2,
                                               ASCIIToUTF16("OF2"));
    model_->AddURL(of2, 0, ASCIIToUTF16("of2a"), GURL(test_base + "of2a"));
    model_->AddURL(of2, 1, ASCIIToUTF16("of2b"), GURL(test_base + "of2b"));
  }

  gfx::Size bb_view_pref_;
  scoped_ptr<TestingProfile> profile_;
  BrowserThread ui_thread_;
  BrowserThread file_thread_;
  ViewsDelegateImpl views_delegate_;
};

// Clicks on first menu, makes sure button is depressed. Moves mouse to first
// child, clicks it and makes sure a navigation occurs.
class BookmarkBarViewTest1 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = bb_view_->GetBookmarkButton(0);
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
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
    views::TextButton* button = bb_view_->GetBookmarkButton(0);
    ASSERT_TRUE(button->state() == views::CustomButton::BS_PUSHED);

    // Click on the 2nd menu item (A URL).
    ASSERT_TRUE(menu->GetSubmenu());

    views::MenuItemView* menu_to_select =
        menu->GetSubmenu()->GetMenuItemAt(0);
    ui_controls::MoveMouseToCenterAndPress(menu_to_select, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest1::Step3));
  }

  void Step3() {
    // We should have navigated to URL f1a.
    ASSERT_TRUE(navigator_.url_ ==
                model_->GetBookmarkBarNode()->GetChild(0)->GetChild(0)->
                GetURL());

    // Make sure button is no longer pushed.
    views::TextButton* button = bb_view_->GetBookmarkButton(0);
    ASSERT_TRUE(button->state() == views::CustomButton::BS_NORMAL);

    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu == NULL || !menu->GetSubmenu()->IsShowing());

    Done();
  }
};

// TODO(jcampan): http://crbug.com/26996 temporarily disabled because failing
//                since we move to running the process
VIEW_TEST(BookmarkBarViewTest1, DISABLED_Basic)

// Brings up menu, clicks on empty space and make sure menu hides.
class BookmarkBarViewTest2 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = bb_view_->GetBookmarkButton(0);
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
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
    views::View::ConvertPointToScreen(bb_view_, &mouse_loc);
    ui_controls::SendMouseMove(0, 0);
    ui_controls::SendMouseEventsNotifyWhenDone(
        ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest2::Step3));
  }

  void Step3() {
    // The menu shouldn't be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu == NULL || !menu->GetSubmenu()->IsShowing());

    // Make sure button is no longer pushed.
    views::TextButton* button = bb_view_->GetBookmarkButton(0);
    ASSERT_TRUE(button->state() == views::CustomButton::BS_NORMAL);

    window_->Activate();

    Done();
  }
};

// TODO(jcampan): http://crbug.com/26996 temporarily disabled because failing
//                since we move to running the process
VIEW_TEST(BookmarkBarViewTest2, DISABLED_HideOnDesktopClick)

// Brings up menu. Moves over child to make sure submenu appears, moves over
// another child and make sure next menu appears.
class BookmarkBarViewTest3 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::MenuButton* button = bb_view_->other_bookmarked_button();
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
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
    ui_controls::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest3::Step3));
  }

  void Step3() {
    // Make sure sub menu is showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    views::MenuItemView* child_menu =
        menu->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_TRUE(child_menu->GetSubmenu() != NULL);
    ASSERT_TRUE(child_menu->GetSubmenu()->IsShowing());

    // Click on third child, which has a submenu too.
    child_menu = menu->GetSubmenu()->GetMenuItemAt(2);
    ASSERT_TRUE(child_menu != NULL);
    ui_controls::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
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
class ContextMenuNotificationObserver : public NotificationObserver {
 public:
  explicit ContextMenuNotificationObserver(Task* task) : task_(task) {
    registrar_.Add(this,
                   NotificationType::BOOKMARK_CONTEXT_MENU_SHOWN,
                   NotificationService::AllSources());
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    MessageLoop::current()->PostTask(FROM_HERE, task_);
  }

 private:
  NotificationRegistrar registrar_;
  Task* task_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuNotificationObserver);
};

// Tests context menus by way of opening a context menu for a bookmark,
// then right clicking to get context menu and selecting the first menu item
// (open).
class BookmarkBarViewTest4 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest4()
      : ALLOW_THIS_IN_INITIALIZER_LIST(
          observer_(CreateEventTask(this, &BookmarkBarViewTest4::Step3))) {
  }

 protected:
  virtual void DoTestOnMessageLoop() {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = bb_view_->other_bookmarked_button();
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
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
    ui_controls::MoveMouseToCenterAndPress(child_menu, ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP, NULL);
    // Step3 will be invoked by ContextMenuNotificationObserver.
  }

  void Step3() {
    // Make sure the context menu is showing.
    views::MenuItemView* menu = bb_view_->GetContextMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu());
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Select the first menu item (open).
    ui_controls::MoveMouseToCenterAndPress(menu->GetSubmenu()->GetMenuItemAt(0),
        ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest4::Step4));
  }

  void Step4() {
    EXPECT_EQ(navigator_.url_,
              model_->other_node()->GetChild(0)->GetURL());

    Done();
  }

  ContextMenuNotificationObserver observer_;
};

VIEW_TEST(BookmarkBarViewTest4, ContextMenus)

// Tests drag and drop within the same menu.
class BookmarkBarViewTest5 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() {
    url_dragging_ =
        model_->GetBookmarkBarNode()->GetChild(0)->GetChild(0)->GetURL();

    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = bb_view_->GetBookmarkButton(0);
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
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
    ui_controls::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
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
    gfx::Point loc(1, target_menu->height() - 1);
    views::View::ConvertPointToScreen(target_menu, &loc);
    ui_controls::SendMouseMove(loc.x(), loc.y());

    ui_controls::SendMouseEventsNotifyWhenDone(ui_controls::LEFT,
        ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest5::Step5));
  }

  void Step5() {
    GURL url = model_->GetBookmarkBarNode()->GetChild(0)->GetChild(1)->GetURL();
    EXPECT_EQ(url_dragging_, url);
    Done();
  }

  GURL url_dragging_;
};

VIEW_TEST(BookmarkBarViewTest5, MAYBE_DND)

// Tests holding mouse down on overflow button, dragging such that menu pops up
// then selecting an item.
class BookmarkBarViewTest6 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() {
    // Press the mouse button on the overflow button. Don't release it though.
    views::TextButton* button = bb_view_->overflow_button();
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
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
    ui_controls::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
        ui_controls::UP, CreateEventTask(this, &BookmarkBarViewTest6::Step3));
  }

  void Step3() {
    ASSERT_TRUE(navigator_.url_ ==
                model_->GetBookmarkBarNode()->GetChild(4)->GetURL());
    Done();
  }

  GURL url_dragging_;
};

VIEW_TEST(BookmarkBarViewTest6, OpenMenuOnClickAndHold)

// Tests drag and drop to different menu.
class BookmarkBarViewTest7 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() {
    url_dragging_ =
        model_->GetBookmarkBarNode()->GetChild(0)->GetChild(0)->GetURL();

    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = bb_view_->GetBookmarkButton(0);
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
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
    ui_controls::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
        ui_controls::DOWN,
        CreateEventTask(this, &BookmarkBarViewTest7::Step3));
  }

  void Step3() {
    // Drag over other button.
    views::TextButton* other_button =
        bb_view_->other_bookmarked_button();
    gfx::Point loc(other_button->width() / 2, other_button->height() / 2);
    views::View::ConvertPointToScreen(other_button, &loc);

    // Start a drag.
    ui_controls::SendMouseMoveNotifyWhenDone(loc.x() + 10, loc.y(),
        NewRunnableMethod(this, &BookmarkBarViewTest7::Step4));

    // See comment above this method as to why we do this.
    ScheduleMouseMoveInBackground(loc.x(), loc.y());
  }

  void Step4() {
    views::MenuItemView* drop_menu = bb_view_->GetDropMenu();
    ASSERT_TRUE(drop_menu != NULL);
    ASSERT_TRUE(drop_menu->GetSubmenu()->IsShowing());

    views::MenuItemView* target_menu =
        drop_menu->GetSubmenu()->GetMenuItemAt(0);
    gfx::Point loc(1, 1);
    views::View::ConvertPointToScreen(target_menu, &loc);
    ui_controls::SendMouseMove(loc.x(), loc.y());
    ui_controls::SendMouseEventsNotifyWhenDone(
        ui_controls::LEFT, ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest7::Step5));
  }

  void Step5() {
    ASSERT_TRUE(model_->other_node()->GetChild(0)->GetURL() == url_dragging_);
    Done();
  }

  GURL url_dragging_;
};

VIEW_TEST(BookmarkBarViewTest7, MAYBE_DNDToDifferentMenu)

// Drags from one menu to next so that original menu closes, then back to
// original menu.
class BookmarkBarViewTest8 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() {
    url_dragging_ =
        model_->GetBookmarkBarNode()->GetChild(0)->GetChild(0)->GetURL();

    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = bb_view_->GetBookmarkButton(0);
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
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
    ui_controls::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
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
    ui_controls::SendMouseMoveNotifyWhenDone(loc.x() + 10, loc.y(),
        NewRunnableMethod(this, &BookmarkBarViewTest8::Step4));

    // See comment above this method as to why we do this.
    ScheduleMouseMoveInBackground(loc.x(), loc.y());
  }

  void Step4() {
    views::MenuItemView* drop_menu = bb_view_->GetDropMenu();
    ASSERT_TRUE(drop_menu != NULL);
    ASSERT_TRUE(drop_menu->GetSubmenu()->IsShowing());

    // Now drag back over first menu.
    views::TextButton* button = bb_view_->GetBookmarkButton(0);
    gfx::Point loc(button->width() / 2, button->height() / 2);
    views::View::ConvertPointToScreen(button, &loc);
    ui_controls::SendMouseMoveNotifyWhenDone(loc.x(), loc.y(),
        NewRunnableMethod(this, &BookmarkBarViewTest8::Step5));
  }

  void Step5() {
    // Drop on folder F11.
    views::MenuItemView* drop_menu = bb_view_->GetDropMenu();
    ASSERT_TRUE(drop_menu != NULL);
    ASSERT_TRUE(drop_menu->GetSubmenu()->IsShowing());

    views::MenuItemView* target_menu =
        drop_menu->GetSubmenu()->GetMenuItemAt(1);
    ui_controls::MoveMouseToCenterAndPress(
        target_menu, ui_controls::LEFT, ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest8::Step6));
  }

  void Step6() {
    // Make sure drop was processed.
    GURL final_url = model_->GetBookmarkBarNode()->GetChild(0)->GetChild(0)->
        GetChild(1)->GetURL();
    ASSERT_TRUE(final_url == url_dragging_);
    Done();
  }

  GURL url_dragging_;
};

VIEW_TEST(BookmarkBarViewTest8, MAYBE_DNDBackToOriginatingMenu)

// Moves the mouse over the scroll button and makes sure we get scrolling.
class BookmarkBarViewTest9 : public BookmarkBarViewEventTestBase {
 protected:
  virtual bool CreateBigMenu() { return true; }

  virtual void DoTestOnMessageLoop() {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = bb_view_->GetBookmarkButton(0);
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
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
    views::View* scroll_down_button = scroll_container->GetChildViewAt(1);
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
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        NewRunnableMethod(this, &BookmarkBarViewTest9::Step4), 200);
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
    MessageLoop::current()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &ViewEventTestBase::Done));
  }

  int start_y_;
  views::MenuItemView* first_menu_;
};

VIEW_TEST(BookmarkBarViewTest9, MAYBE_ScrollButtonScrolls)

// Tests up/down/left/enter key messages.
class BookmarkBarViewTest10 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = bb_view_->GetBookmarkButton(0);
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest10::Step2));
    MessageLoop::current()->RunAllPending();
  }

 private:
  void Step2() {
    // Menu should be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu != NULL);
    ASSERT_TRUE(menu->GetSubmenu()->IsShowing());

    // Send a down event, which should select the first item.
    ui_controls::SendKeyPressNotifyWhenDone(
        NULL, ui::VKEY_DOWN, false, false, false, false,
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
        NULL, ui::VKEY_DOWN, false, false, false, false,
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
        NULL, ui::VKEY_RIGHT, false, false, false, false,
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
        NULL, ui::VKEY_LEFT, false, false, false, false,
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
        NULL, ui::VKEY_DOWN, false, false, false, false,
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
        NULL, ui::VKEY_RETURN, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest10::Step8));
  }

  void Step8() {
    ASSERT_TRUE(
        model_->GetBookmarkBarNode()->GetChild(0)->GetChild(0)->GetURL() ==
        navigator_.url_);
    Done();
  }
};

VIEW_TEST(BookmarkBarViewTest10, MAYBE_KeyEvents)

// Make sure the menu closes with the following sequence: show menu, show
// context menu, close context menu (via escape), then click else where. This
// effectively verifies we maintain mouse capture after the context menu is
// hidden.
class BookmarkBarViewTest11 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest11()
      : ALLOW_THIS_IN_INITIALIZER_LIST(
          observer_(CreateEventTask(this, &BookmarkBarViewTest11::Step3))) {
  }

 protected:
  virtual void DoTestOnMessageLoop() {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = bb_view_->other_bookmarked_button();
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
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
    ui_controls::MoveMouseToCenterAndPress(child_menu, ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP, NULL);
    // Step3 will be invoked by ContextMenuNotificationObserver.
  }

  void Step3() {
    // Send escape so that the context menu hides.
    ui_controls::SendKeyPressNotifyWhenDone(
        NULL, ui::VKEY_ESCAPE, false, false, false, false,
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
    views::View::ConvertPointToScreen(bb_view_, &mouse_loc);
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

VIEW_TEST(BookmarkBarViewTest11, MAYBE_CloseMenuAfterClosingContextMenu)

// Tests showing a modal dialog from a context menu.
class BookmarkBarViewTest12 : public BookmarkBarViewEventTestBase {
 protected:
  virtual void DoTestOnMessageLoop() {
    // Open up the other folder.
    views::TextButton* button = bb_view_->other_bookmarked_button();
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest12::Step2));
    bookmark_utils::num_urls_before_prompting = 1;
  }

  ~BookmarkBarViewTest12() {
    bookmark_utils::num_urls_before_prompting = 15;
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
    ui_controls::MoveMouseToCenterAndPress(child_menu, ui_controls::RIGHT,
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
    ui_controls::MoveMouseToCenterAndPress(child_menu, ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP, NULL);

    // Delay until we send tab, otherwise the message box doesn't appear
    // correctly.
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        CreateEventTask(this, &BookmarkBarViewTest12::Step4), 1000);
  }

  void Step4() {
    // Press tab to give focus to the cancel button.
    ui_controls::SendKeyPress(NULL, ui::VKEY_TAB, false, false, false, false);

    // For some reason return isn't processed correctly unless we delay.
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        CreateEventTask(this, &BookmarkBarViewTest12::Step5), 1000);
  }

  void Step5() {
    // And press enter so that the cancel button is selected.
    ui_controls::SendKeyPressNotifyWhenDone(
        NULL, ui::VKEY_RETURN, false, false, false, false,
        CreateEventTask(this, &BookmarkBarViewTest12::Step6));
  }

  void Step6() {
    // Do a delayed task to give the dialog time to exit.
    MessageLoop::current()->PostTask(
        FROM_HERE, CreateEventTask(this, &BookmarkBarViewTest12::Step7));
  }

  void Step7() {
    Done();
  }
};

VIEW_TEST(BookmarkBarViewTest12, MAYBE_CloseWithModalDialog)

// Tests clicking on the separator of a context menu (this is for coverage of
// bug 17862).
class BookmarkBarViewTest13 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest13()
      : ALLOW_THIS_IN_INITIALIZER_LIST(
          observer_(CreateEventTask(this, &BookmarkBarViewTest13::Step3))) {
  }

 protected:
  virtual void DoTestOnMessageLoop() {
    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = bb_view_->other_bookmarked_button();
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
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
    ui_controls::MoveMouseToCenterAndPress(child_menu, ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP, NULL);
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
      if (submenu->GetChildViewAt(i)->GetID() !=
          views::MenuItemView::kMenuItemViewID) {
        separator_view = submenu->GetChildViewAt(i);
        break;
      }
    }
    ASSERT_TRUE(separator_view);

    // Click on the separator. Clicking on the separator shouldn't visually
    // change anything.
    ui_controls::MoveMouseToCenterAndPress(separator_view,
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
    ui_controls::MoveMouseToCenterAndPress(menu->GetSubmenu()->GetMenuItemAt(0),
        ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &BookmarkBarViewTest13::Step5));
  }

  void Step5() {
    Done();
  }

  ContextMenuNotificationObserver observer_;
};

VIEW_TEST(BookmarkBarViewTest13, ClickOnContextMenuSeparator)

// Makes sure right cliking on a folder on the bookmark bar doesn't result in
// both a context menu and showing the menu.
class BookmarkBarViewTest14 : public BookmarkBarViewEventTestBase {
 public:
  BookmarkBarViewTest14()
      : ALLOW_THIS_IN_INITIALIZER_LIST(
          observer_(CreateEventTask(this, &BookmarkBarViewTest14::Step2))) {
  }

 protected:
  virtual void DoTestOnMessageLoop() {
    // Move the mouse to the first folder on the bookmark bar and press the
    // right mouse button.
    views::TextButton* button = bb_view_->GetBookmarkButton(0);
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP, NULL);
    // Step2 will be invoked by ContextMenuNotificationObserver.
  }

 private:

  void Step2() {
    // Menu should NOT be showing.
    views::MenuItemView* menu = bb_view_->GetMenu();
    ASSERT_TRUE(menu == NULL);

    // Send escape so that the context menu hides.
    ui_controls::SendKeyPressNotifyWhenDone(
        NULL, ui::VKEY_ESCAPE, false, false, false, false,
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
        ALLOW_THIS_IN_INITIALIZER_LIST(
            observer_(CreateEventTask(this, &BookmarkBarViewTest15::Step3))) {
  }

 protected:
  virtual void DoTestOnMessageLoop() {
    // Show the other bookmarks.
    views::TextButton* button = bb_view_->other_bookmarked_button();
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
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
    ui_controls::MoveMouseToCenterAndPress(child_menu, ui_controls::RIGHT,
        ui_controls::DOWN | ui_controls::UP, NULL);
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
    ui_controls::MoveMouseToCenterAndPress(delete_menu,
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
  virtual void DoTestOnMessageLoop() {
    InstallViewsDelegate();

    // Move the mouse to the first folder on the bookmark bar and press the
    // mouse.
    views::TextButton* button = bb_view_->GetBookmarkButton(0);
    ui_controls::MoveMouseToCenterAndPress(button, ui_controls::LEFT,
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
    views::TextButton* button = bb_view_->GetBookmarkButton(0);
    ASSERT_TRUE(button->state() == views::CustomButton::BS_PUSHED);

    // Close the window.
    window_->Close();
    window_ = NULL;
  }
};

// Disabled, http://crbug.com/64303.
VIEW_TEST(BookmarkBarViewTest16, DISABLED_DeleteMenu)

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_bubble_sign_in_delegate.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/bookmarks/bookmark_bubble_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/range/range.h"

class BookmarkBubbleSignInDelegateTest : public BrowserWithTestWindowTest {
 public:
  BookmarkBubbleSignInDelegateTest() {}

 protected:
  class Window : public TestBrowserWindow {
   public:
    Window() : show_count_(0) {}

    int show_count() { return show_count_; }

   private:
    // TestBrowserWindow:
    virtual void Show() OVERRIDE {
      ++show_count_;
    }

    // Number of times that the Show() method has been called.
    int show_count_;

    DISALLOW_COPY_AND_ASSIGN(Window);
  };

  virtual BrowserWindow* CreateBrowserWindow() OVERRIDE {
    return new Window();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkBubbleSignInDelegateTest);
};

TEST_F(BookmarkBubbleSignInDelegateTest, OnSignInLinkClicked) {
  int starting_tab_count = browser()->tab_strip_model()->count();

  scoped_ptr<BookmarkBubbleDelegate> delegate;
  delegate.reset(new BookmarkBubbleSignInDelegate(browser()));

  delegate->OnSignInLinkClicked();

  // A new tab should have been opened and the browser should be visible.
  int tab_count = browser()->tab_strip_model()->count();
  EXPECT_EQ(starting_tab_count + 1, tab_count);
  EXPECT_EQ(1,
            static_cast<BookmarkBubbleSignInDelegateTest::Window*>(
                browser()->window())->show_count());
}

TEST_F(BookmarkBubbleSignInDelegateTest, OnSignInLinkClickedIncognito) {
  // Create an incognito browser.
  TestingProfile::Builder incognito_profile_builder;
  incognito_profile_builder.SetIncognito();
  scoped_ptr<TestingProfile> incognito_profile =
      incognito_profile_builder.Build();
  incognito_profile->SetOriginalProfile(profile());
  profile()->SetOffTheRecordProfile(incognito_profile.PassAs<Profile>());

  scoped_ptr<BrowserWindow> incognito_window;
  incognito_window.reset(CreateBrowserWindow());
  Browser::CreateParams params(browser()->profile()->GetOffTheRecordProfile(),
                               browser()->host_desktop_type());
  params.window = incognito_window.get();
  scoped_ptr<Browser> incognito_browser;
  incognito_browser.reset(new Browser(params));

  int starting_tab_count_normal = browser()->tab_strip_model()->count();
  int starting_tab_count_incognito =
      incognito_browser.get()->tab_strip_model()->count();

  scoped_ptr<BookmarkBubbleDelegate> delegate;
  delegate.reset(new BookmarkBubbleSignInDelegate(incognito_browser.get()));

  delegate->OnSignInLinkClicked();

  // A new tab should have been opened in the normal browser, which should be
  // visible.
  int tab_count_normal = browser()->tab_strip_model()->count();
  EXPECT_EQ(starting_tab_count_normal + 1, tab_count_normal);
  EXPECT_EQ(1,
            static_cast<BookmarkBubbleSignInDelegateTest::Window*>(
                browser()->window())->show_count());

  // No effect is expected on the incognito browser.
  int tab_count_incognito = incognito_browser->tab_strip_model()->count();
  EXPECT_EQ(starting_tab_count_incognito, tab_count_incognito);
  EXPECT_EQ(0,
            static_cast<BookmarkBubbleSignInDelegateTest::Window*>(
                incognito_window.get())->show_count());
}

// Verifies that the sign in page can be loaded in a different browser
// if the provided browser is invalidated.
TEST_F(BookmarkBubbleSignInDelegateTest, BrowserRemoved) {
  // Create an extra browser.
  scoped_ptr<BrowserWindow> extra_window;
  extra_window.reset(CreateBrowserWindow());

  Browser::CreateParams params(browser()->profile(),
                               browser()->host_desktop_type());
  params.window = extra_window.get();
  scoped_ptr<Browser> extra_browser;
  extra_browser.reset(new Browser(params));

  int starting_tab_count = extra_browser->tab_strip_model()->count();

  scoped_ptr<BookmarkBubbleDelegate> delegate;
  delegate.reset(new BookmarkBubbleSignInDelegate(browser()));

  BrowserList::SetLastActive(extra_browser.get());

  browser()->tab_strip_model()->CloseAllTabs();
  set_browser(NULL);

  delegate->OnSignInLinkClicked();

  // A new tab should have been opened in the extra browser, which should be
  // visible.
  int tab_count = extra_browser->tab_strip_model()->count();
  EXPECT_EQ(starting_tab_count + 1, tab_count);
  EXPECT_EQ(1,
            static_cast<BookmarkBubbleSignInDelegateTest::Window*>(
                extra_window.get())->show_count());

  // Required to avoid a crash when the browser is deleted.
  extra_browser->tab_strip_model()->CloseAllTabs();
}

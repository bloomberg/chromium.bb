// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_iterator.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/test/base/browser_with_test_window_test.h"

typedef BrowserWithTestWindowTest BrowserIteratorTest;

namespace {

// BrowserWithTestWindowTest's default window is on the native desktop by
// default. Force it to be on the Ash desktop to be able to test the iterator
// in a situation with no browsers on the native desktop.
class BrowserIteratorTestWithInitialWindowInAsh
    : public BrowserWithTestWindowTest {
 public:
  BrowserIteratorTestWithInitialWindowInAsh()
      : BrowserWithTestWindowTest(chrome::HOST_DESKTOP_TYPE_ASH) {
  }
};

// Helper function to iterate and count all the browsers.
size_t CountAllBrowsers() {
  size_t count = 0;
  for (chrome::BrowserIterator iterator; !iterator.done(); iterator.Next())
    ++count;
  return count;
}

}

TEST_F(BrowserIteratorTest, BrowsersOnMultipleDesktops) {
  Browser::CreateParams native_params(profile(),
                                      chrome::HOST_DESKTOP_TYPE_NATIVE);
  Browser::CreateParams ash_params(profile(), chrome::HOST_DESKTOP_TYPE_ASH);
  // Note, browser 1 is on the native desktop.
  scoped_ptr<Browser> browser2(
      chrome::CreateBrowserWithTestWindowForParams(&native_params));
  scoped_ptr<Browser> browser3(
      chrome::CreateBrowserWithTestWindowForParams(&native_params));
  scoped_ptr<Browser> browser4(
      chrome::CreateBrowserWithTestWindowForParams(&ash_params));
  scoped_ptr<Browser> browser5(
      chrome::CreateBrowserWithTestWindowForParams(&ash_params));

  // Sanity checks.
  BrowserList* native_list =
      BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_NATIVE);
#if defined(OS_CHROMEOS)
  // On Chrome OS, the native list and the ash list are the same.
  EXPECT_EQ(5U, native_list->size());
#else
  BrowserList* ash_list =
      BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);
  EXPECT_EQ(3U, native_list->size());
  EXPECT_EQ(2U, ash_list->size());
#endif

  // Make sure the iterator finds all 5 browsers regardless of which desktop
  // they are on.
  EXPECT_EQ(5U, CountAllBrowsers());
}

TEST_F(BrowserIteratorTest, NoBrowsersOnAshDesktop) {
  Browser::CreateParams native_params(profile(),
                                      chrome::HOST_DESKTOP_TYPE_NATIVE);
  // Note, browser 1 is on the native desktop.
  scoped_ptr<Browser> browser2(
      chrome::CreateBrowserWithTestWindowForParams(&native_params));
  scoped_ptr<Browser> browser3(
      chrome::CreateBrowserWithTestWindowForParams(&native_params));

  // Sanity checks.
  BrowserList* native_list =
      BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_NATIVE);
  EXPECT_EQ(3U, native_list->size());
#if !defined(OS_CHROMEOS)
  // On non-Chrome OS platforms the Ash list is independent from the native
  // list, double-check that it's empty.
  BrowserList* ash_list =
      BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);
  EXPECT_EQ(0U, ash_list->size());
#endif

  // Make sure the iterator finds all 3 browsers on the native desktop and
  // doesn't trip over the empty Ash desktop list.
  EXPECT_EQ(3U, CountAllBrowsers());
}

TEST_F(BrowserIteratorTestWithInitialWindowInAsh, NoBrowsersOnNativeDesktop) {
  Browser::CreateParams ash_params(profile(), chrome::HOST_DESKTOP_TYPE_ASH);
  // Note, browser 1 is on the ash desktop.
  scoped_ptr<Browser> browser2(
      chrome::CreateBrowserWithTestWindowForParams(&ash_params));
  scoped_ptr<Browser> browser3(
      chrome::CreateBrowserWithTestWindowForParams(&ash_params));

  BrowserList* ash_list =
      BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);
  EXPECT_EQ(3U, ash_list->size());
#if !defined(OS_CHROMEOS)
  // On non-Chrome OS platforms the native list is independent from the Ash
  // list; double-check that it's empty.
  BrowserList* native_list =
      BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_NATIVE);
  EXPECT_EQ(0U, native_list->size());
#endif

  // Make sure the iterator finds all 3 browsers on the ash desktop and
  // doesn't trip over the empty native desktop list.
  EXPECT_EQ(3U, CountAllBrowsers());
}

// Verify that the iterator still behaves if there are no browsers at all.
TEST(BrowserIteratorTestWithNoTestWindow, NoBrowser) {
  EXPECT_EQ(0U, CountAllBrowsers());
}

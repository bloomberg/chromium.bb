// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/star_view.h"

#include "chrome/browser/ui/views/browser_dialogs.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace {

typedef InProcessBrowserTest StarViewTest;

// Verify that clicking the bookmark star a second time hides the bookmark
// bubble.
#if defined(OS_LINUX) && defined(USE_AURA) && !defined(OS_CHROMEOS)
#define MAYBE_HideOnSecondClick DISABLED_HideOnSecondClick
#else
#define MAYBE_HideOnSecondClick HideOnSecondClick
#endif
IN_PROC_BROWSER_TEST_F(StarViewTest, MAYBE_HideOnSecondClick) {
  BrowserView* browser_view = reinterpret_cast<BrowserView*>(
      browser()->window());
  views::ImageView* star_view =
      browser_view->GetToolbarView()->location_bar()->star_view();

  ui::MouseEvent pressed_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
      ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent released_event(ui::ET_MOUSE_RELEASED, gfx::Point(),
      gfx::Point(), ui::EF_LEFT_MOUSE_BUTTON);

  // Verify that clicking once shows the bookmark bubble.
  EXPECT_FALSE(chrome::IsBookmarkBubbleViewShowing());
  star_view->OnMousePressed(pressed_event);
  EXPECT_FALSE(chrome::IsBookmarkBubbleViewShowing());
  star_view->OnMouseReleased(released_event);
  EXPECT_TRUE(chrome::IsBookmarkBubbleViewShowing());

  // Verify that clicking again doesn't reshow it.
  star_view->OnMousePressed(pressed_event);
  // Hide the bubble manually. In the browser this would normally happen during
  // the event processing.
  chrome::HideBookmarkBubbleView();
  MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(chrome::IsBookmarkBubbleViewShowing());
  star_view->OnMouseReleased(released_event);
  EXPECT_FALSE(chrome::IsBookmarkBubbleViewShowing());
}

}  // namespace

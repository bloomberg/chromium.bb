// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/browser_non_client_frame_view_ash.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/views/widget/widget.h"

using views::Widget;

typedef InProcessBrowserTest BrowserNonClientFrameViewAshTest;

IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewAshTest, UseShortHeader) {
  // We know we're using Views, so static cast.
  Widget* widget = static_cast<BrowserView*>(browser()->window())->GetWidget();
  // We know we're using Ash, so static cast.
  BrowserNonClientFrameViewAsh* frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          widget->non_client_view()->frame_view());
  // Restored window uses tall header.
  widget->SetBounds(gfx::Rect(10, 10, 300, 300));
  EXPECT_FALSE(frame_view->UseShortHeader());
  // Window at top of screen uses short header.
  widget->SetBounds(gfx::Rect(10, 0, 300, 300));
  EXPECT_TRUE(frame_view->UseShortHeader());
  // Maximized window uses short header.
  widget->Maximize();
  EXPECT_TRUE(frame_view->UseShortHeader());

  // Popups use short header.
  Browser* popup = CreateBrowserForPopup(browser()->profile());
  Widget* popup_widget =
      static_cast<BrowserView*>(popup->window())->GetWidget();
  BrowserNonClientFrameViewAsh* popup_frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          popup_widget->non_client_view()->frame_view());
  popup_widget->SetBounds(gfx::Rect(5, 5, 200, 200));
  EXPECT_TRUE(popup_frame_view->UseShortHeader());

  // Apps use tall header.
  Browser* app = CreateBrowserForApp("name", browser()->profile());
  Widget* app_widget =
      static_cast<BrowserView*>(app->window())->GetWidget();
  BrowserNonClientFrameViewAsh* app_frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          app_widget->non_client_view()->frame_view());
  app_widget->SetBounds(gfx::Rect(15, 15, 250, 250));
  EXPECT_FALSE(app_frame_view->UseShortHeader());
}

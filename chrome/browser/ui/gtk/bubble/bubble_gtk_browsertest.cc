// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/base/gtk/gtk_hig_constants.h"

class BubbleGtkTest : public InProcessBrowserTest,
                      public BubbleDelegateGtk {
 public:
  BubbleGtkTest() : browser_window_(NULL) {
  }

  virtual ~BubbleGtkTest() {
  }

  // BubbleDelegateGtk implementation.
  virtual void BubbleClosing(BubbleGtk* bubble,
                             bool closed_by_escape) OVERRIDE {
  }

  Profile* GetProfile() {
    return browser()->profile();
  }

  GtkWidget* GetNativeBrowserWindow() {
    if (!browser_window_)
      browser_window_ = GTK_WIDGET(browser()->window()->GetNativeWindow());
    return browser_window_;
  }

 private:
  GtkWidget* browser_window_;
};

// Tests that we can adjust a bubble arrow so we can show a bubble without being
// clipped. This test verifies the following four issues:
// 1. Shows a bubble to the top-left corner and see its frame style always
// becomes ANCHOR_TOP_LEFT.
// 2. Shows a bubble to the top-right corner and see its frame style always
// becomes ANCHOR_TOP_RIGHT.
// 3. Shows a bubble to the bottom-left corner and see its frame style always
// becomes ANCHOR_BOTTOM_LEFT.
// 4. Shows a bubble to the top-left corner and see its frame style always
// becomes ANCHOR_BOTTOM_RIGHT.
IN_PROC_BROWSER_TEST_F(BubbleGtkTest, ArrowLocation) {
  int width = gdk_screen_get_width(gdk_screen_get_default());
  int height = gdk_screen_get_height(gdk_screen_get_default());
  struct {
    int x, y;
    BubbleGtk::FrameStyle expected;
  } points[] = {
    {0, 0, BubbleGtk::ANCHOR_TOP_LEFT},
    {width - 1, 0, BubbleGtk::ANCHOR_TOP_RIGHT},
    {0, height - 1, BubbleGtk::ANCHOR_BOTTOM_LEFT},
    {width - 1, height - 1, BubbleGtk::ANCHOR_BOTTOM_RIGHT},
  };
  static const BubbleGtk::FrameStyle kPreferredLocations[] = {
    BubbleGtk::ANCHOR_TOP_LEFT,
    BubbleGtk::ANCHOR_TOP_RIGHT,
    BubbleGtk::ANCHOR_BOTTOM_LEFT,
    BubbleGtk::ANCHOR_BOTTOM_RIGHT,
  };

  GtkWidget* anchor = GetNativeBrowserWindow();
  GtkThemeService* theme_service = GtkThemeService::GetFrom(GetProfile());
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(points); ++i) {
    for (size_t j = 0; j < arraysize(kPreferredLocations); ++j) {
      static const char kText[] =
          "Google's mission is to organize the world's information and make it"
          " universally accessible and useful.";
      GtkWidget* label = theme_service->BuildLabel(kText, ui::kGdkBlack);
      gfx::Rect rect(points[i].x, points[i].y, 0, 0);
      BubbleGtk* bubble = BubbleGtk::Show(anchor,
                                          &rect,
                                          label,
                                          kPreferredLocations[j],
                                          BubbleGtk::MATCH_SYSTEM_THEME |
                                              BubbleGtk::POPUP_WINDOW |
                                              BubbleGtk::GRAB_INPUT,
                                          theme_service,
                                          this);
      EXPECT_EQ(points[i].expected, bubble->actual_frame_style_);
      bubble->Close();
    }
  }
}

IN_PROC_BROWSER_TEST_F(BubbleGtkTest, NoArrow) {
  int width = gdk_screen_get_width(gdk_screen_get_default());
  int height = gdk_screen_get_height(gdk_screen_get_default());
  struct {
    int x, y;
    BubbleGtk::FrameStyle expected;
  } points[] = {
    {0, 0, BubbleGtk::FLOAT_BELOW_RECT},
    {width - 1, 0, BubbleGtk::FLOAT_BELOW_RECT},
    {0, height - 1, BubbleGtk::CENTER_OVER_RECT},
    {width - 1, height - 1, BubbleGtk::CENTER_OVER_RECT},
  };
  static const BubbleGtk::FrameStyle kPreferredLocations[] = {
    BubbleGtk::FLOAT_BELOW_RECT,
    BubbleGtk::CENTER_OVER_RECT,
  };

  GtkWidget* anchor = GetNativeBrowserWindow();
  GtkThemeService* theme_service = GtkThemeService::GetFrom(GetProfile());
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(points); ++i) {
    for (size_t j = 0; j < arraysize(kPreferredLocations); ++j) {
      static const char kText[] =
          "Google's mission is to organize the world's information and make it"
          " universally accessible and useful.";
      GtkWidget* label = theme_service->BuildLabel(kText, ui::kGdkBlack);
      gfx::Rect rect(points[i].x, points[i].y, 0, 0);
      BubbleGtk* bubble = BubbleGtk::Show(anchor,
                                          &rect,
                                          label,
                                          kPreferredLocations[j],
                                          BubbleGtk::MATCH_SYSTEM_THEME |
                                              BubbleGtk::POPUP_WINDOW |
                                              BubbleGtk::GRAB_INPUT,
                                          theme_service,
                                          this);
      EXPECT_EQ(points[i].expected, bubble->actual_frame_style_);
      bubble->Close();
    }
  }
}

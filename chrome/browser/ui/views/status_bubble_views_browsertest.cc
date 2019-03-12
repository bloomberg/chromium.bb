// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_bubble_views.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/status_bubble_views_browsertest_mac.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

class StatusBubbleViewsTest : public InProcessBrowserTest {
 public:
  StatusBubble* GetBubble() { return browser()->window()->GetStatusBubble(); }
  views::Widget* GetWidget() {
    return static_cast<StatusBubbleViews*>(GetBubble())->popup();
  }
};

IN_PROC_BROWSER_TEST_F(StatusBubbleViewsTest, WidgetLifetime) {
  // The widget does not exist until it needs to be shown.
  StatusBubble* bubble = GetBubble();
  ASSERT_TRUE(bubble);
  EXPECT_FALSE(GetWidget());

  // Setting status text shows the widget.
  bubble->SetStatus(base::ASCIIToUTF16("test"));
  views::Widget* widget = GetWidget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Changing status text keeps the widget visible.
  bubble->SetStatus(base::ASCIIToUTF16("foo"));
  EXPECT_TRUE(widget->IsVisible());

  // Setting the URL keeps the widget visible.
  bubble->SetURL(GURL("http://www.foo.com"));
  EXPECT_TRUE(widget->IsVisible());

#if !defined(OS_MACOSX)
  // Clearing the URL and status closes the widget on platforms other than Mac.
  views::test::WidgetClosingObserver widget_closing_observer(widget);
  bubble->SetStatus(base::string16());
  bubble->SetURL(GURL());
  widget_closing_observer.Wait();
  EXPECT_TRUE(widget_closing_observer.widget_closed());
#endif
}

// Ensure the status bubble does not close itself on Mac. Doing so can trigger
// unwanted space switches due to rdar://9037452. See https://crbug.com/866760.
IN_PROC_BROWSER_TEST_F(StatusBubbleViewsTest, NeverHideOrCloseOnMac) {
  StatusBubble* bubble = GetBubble();
  bubble->SetStatus(base::ASCIIToUTF16("test"));
  views::Widget* widget = GetWidget();
  EXPECT_TRUE(widget->IsVisible());

#if defined(OS_MACOSX)
  // Check alpha on Mac as well. On other platforms it is redundant.
  EXPECT_EQ(1.f, test::GetNativeWindowAlphaValue(widget->GetNativeWindow()));
  bubble->Hide();
  // On Mac, the bubble widget remains visible so it can remain a child window.
  // However, it is fully transparent.
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_EQ(0.f, test::GetNativeWindowAlphaValue(widget->GetNativeWindow()));
#else
  views::test::WidgetClosingObserver widget_closing_observer(widget);
  bubble->Hide();
  widget_closing_observer.Wait();
  EXPECT_TRUE(widget_closing_observer.widget_closed());
#endif
}

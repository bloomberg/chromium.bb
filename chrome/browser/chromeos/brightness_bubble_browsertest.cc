// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/chromeos/brightness_bubble.h"
#include "chrome/browser/chromeos/setting_level_bubble_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/views/view.h"

typedef InProcessBrowserTest BrightnessBubbleTest;

namespace chromeos {

IN_PROC_BROWSER_TEST_F(BrightnessBubbleTest, UpdateWithoutShowing) {
  BrightnessBubble* bubble = BrightnessBubble::GetInstance();
  bubble->UpdateWithoutShowingBubble(20, false);
  EXPECT_EQ(NULL, bubble->view_);
  EXPECT_EQ(20, bubble->current_percent_);
  bubble->UpdateWithoutShowingBubble(30, false);
  EXPECT_EQ(NULL, bubble->view_);
  EXPECT_EQ(20, bubble->current_percent_);
  EXPECT_EQ(30, bubble->target_percent_);
  bubble->UpdateWithoutShowingBubble(40, false);
  EXPECT_EQ(NULL, bubble->view_);
  EXPECT_EQ(40, bubble->target_percent_);
  bubble->ShowBubble(50, true);
  EXPECT_TRUE(bubble->view_->GetWidget()->IsVisible());
  EXPECT_EQ(50, bubble->target_percent_);
  MessageLoop::current()->RunAllPending();
}

}  // namespace chromeos

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/setting_level_bubble_view.h"
#include "chrome/browser/chromeos/volume_bubble.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/views/view.h"

typedef InProcessBrowserTest VolumeBubbleTest;

namespace chromeos {

IN_PROC_BROWSER_TEST_F(VolumeBubbleTest, GetInstanceAndShow) {
  VolumeBubble* bubble1 = VolumeBubble::GetInstance();
  VolumeBubble* bubble2 = VolumeBubble::GetInstance();
  ASSERT_EQ(bubble1, bubble2);

  bubble1->ShowBubble(20, true);
  EXPECT_TRUE(bubble1->view_ != NULL);
  EXPECT_TRUE(bubble1->view_->IsVisible());
  views::View* saved_view = bubble1->view_;
  bubble1->HideBubble();
  EXPECT_EQ(NULL, bubble1->view_);
  bubble1->ShowBubble(20, true);
  EXPECT_TRUE(bubble1->view_ != NULL);
  EXPECT_NE(saved_view, bubble1->view_);
}

}  // namespace chromeos

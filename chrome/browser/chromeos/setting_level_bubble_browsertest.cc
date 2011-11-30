// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/chromeos/setting_level_bubble.h"
#include "chrome/browser/chromeos/setting_level_bubble_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/view.h"


class SettingLevelBubbleTest : public InProcessBrowserTest {
 public:
  SettingLevelBubbleTest() {
    up_icon_.setConfig(SkBitmap::kARGB_8888_Config, 10, 10);
    down_icon_.setConfig(SkBitmap::kARGB_8888_Config, 5, 5);
    mute_icon_.setConfig(SkBitmap::kARGB_8888_Config, 1, 1);
  }

  virtual ~SettingLevelBubbleTest() {}

 protected:
  SkBitmap up_icon_;
  SkBitmap down_icon_;
  SkBitmap mute_icon_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SettingLevelBubbleTest);
};

namespace chromeos {

IN_PROC_BROWSER_TEST_F(SettingLevelBubbleTest, CreateAndUpdate) {
  SettingLevelBubble bubble(&up_icon_,
                            &down_icon_,
                            &mute_icon_);
  EXPECT_EQ(NULL, bubble.view_);
  EXPECT_EQ(&up_icon_, bubble.increase_icon_);
  EXPECT_EQ(&down_icon_, bubble.decrease_icon_);
  EXPECT_EQ(&mute_icon_, bubble.disabled_icon_);

  // Update setting:
  // Old target is 50, new target is 70, set enable = false.
  bubble.ShowBubble(70, false);
  EXPECT_TRUE(bubble.view_->GetWidget()->IsVisible());
  EXPECT_EQ(&mute_icon_, bubble.view_->icon_);
  EXPECT_FALSE(bubble.view_->progress_bar_->IsEnabled());

  // Old target is 70, new target is 30, set enable = true.
  bubble.ShowBubble(30, true);
  EXPECT_EQ(&down_icon_, bubble.view_->icon_);
  EXPECT_TRUE(bubble.view_->progress_bar_->IsEnabled());

  // Old target is 30, new target is 40, set enable = true.
  bubble.ShowBubble(30, true);
  EXPECT_EQ(&up_icon_, bubble.view_->icon_);
  EXPECT_TRUE(bubble.view_->progress_bar_->IsEnabled());

  // Old target is 30, new target is 0, set enable = true.
  bubble.ShowBubble(0, true);
  EXPECT_EQ(&mute_icon_, bubble.view_->icon_);
  EXPECT_TRUE(bubble.view_->progress_bar_->IsEnabled());
  bubble.HideBubble();
  MessageLoop::current()->RunAllPending();
}

IN_PROC_BROWSER_TEST_F(SettingLevelBubbleTest, ShowBubble) {
  // Create setting at 30 percent, enabled.
  SettingLevelBubble bubble(&up_icon_,
                            &down_icon_,
                            &mute_icon_);
  bubble.UpdateWithoutShowingBubble(30, false);
  EXPECT_EQ(NULL, bubble.view_);
  EXPECT_EQ(30, bubble.current_percent_);

  // Show bubble at 40 percent, enabled.
  bubble.ShowBubble(40, true);
  EXPECT_TRUE(bubble.view_ != NULL);
  EXPECT_TRUE(bubble.view_->GetWidget()->IsVisible());

  // Update to 0 percent and close.
  bubble.UpdateWithoutShowingBubble(0, true);
  bubble.OnWidgetClosing(bubble.view_->GetWidget());
  EXPECT_EQ(0, bubble.current_percent_);
}

}  // namespace chromeos

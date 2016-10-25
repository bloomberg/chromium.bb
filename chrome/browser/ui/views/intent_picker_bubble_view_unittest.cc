// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/intent_picker_bubble_view.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/arc/intent_helper/arc_navigation_throttle.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/resources/grit/views_resources.h"
#include "url/gurl.h"

using AppInfo = arc::ArcNavigationThrottle::AppInfo;
using content::WebContents;
using content::OpenURLParams;
using content::Referrer;

class IntentPickerBubbleViewTest : public BrowserWithTestWindowTest {
 public:
  IntentPickerBubbleViewTest() = default;

  void TearDown() override {
    // Make sure the bubble is destroyed before the profile to avoid a crash.
    bubble_.reset();

    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  void CreateBubbleView(bool use_icons) {
    // Pushing a couple of fake apps just to check they are created on the UI.
    app_info_.emplace_back(AppInfo(gfx::Image(), "package_1", "dank app 1"));
    app_info_.emplace_back(AppInfo(gfx::Image(), "package_2", "dank_app_2"));

    if (use_icons)
      FillAppListWithDummyIcons();

    // We create |web_contents| since the Bubble UI has an Observer that
    // depends on this, otherwise it wouldn't work.
    GURL url("http://www.google.com");
    WebContents* web_contents = browser()->OpenURL(
        OpenURLParams(url, Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false));

    bubble_ = IntentPickerBubbleView::CreateBubbleView(
        app_info_, base::Bind(&IntentPickerBubbleViewTest::OnBubbleClosed,
                              base::Unretained(this)),
        web_contents);
  }

  void FillAppListWithDummyIcons() {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    gfx::Image dummy_icon = rb.GetImageNamed(IDR_CLOSE);
    for (auto& app : app_info_)
      app.icon = dummy_icon;
  }

  // Dummy method to be called upon bubble closing.
  void OnBubbleClosed(const std::string& selected_app_package,
                      arc::ArcNavigationThrottle::CloseReason close_reason) {}

  std::unique_ptr<IntentPickerBubbleView> bubble_;
  std::vector<AppInfo> app_info_;

 private:
  DISALLOW_COPY_AND_ASSIGN(IntentPickerBubbleViewTest);
};

// Verifies that we didn't set up an image for any LabelButton.
TEST_F(IntentPickerBubbleViewTest, NullIcons) {
  CreateBubbleView(false);
  size_t size = bubble_->app_info_.size();
  for (size_t i = 0; i < size; ++i) {
    gfx::ImageSkia image = bubble_->GetAppImageForTesting(i);
    EXPECT_TRUE(image.isNull()) << i;
  }
}

// Verifies that all the icons contain a non-null icon.
TEST_F(IntentPickerBubbleViewTest, NonNullIcons) {
  CreateBubbleView(true);
  size_t size = bubble_->app_info_.size();
  for (size_t i = 0; i < size; ++i) {
    gfx::ImageSkia image = bubble_->GetAppImageForTesting(i);
    EXPECT_FALSE(image.isNull()) << i;
  }
}

// Verifies that the bubble contains as many rows as the input. Populated the
// bubble with an arbitrary image in every row.
TEST_F(IntentPickerBubbleViewTest, LabelsPtrVectorSize) {
  CreateBubbleView(true);
  EXPECT_EQ(app_info_.size(), bubble_->app_info_.size());
}

// Verifies the InkDrop state when creating a new bubble.
TEST_F(IntentPickerBubbleViewTest, VerifyStartingInkDrop) {
  CreateBubbleView(true);
  size_t size = bubble_->app_info_.size();
  for (size_t i = 0; i < size; ++i) {
    EXPECT_EQ(bubble_->GetInkDropStateForTesting(i),
              views::InkDropState::HIDDEN);
  }
}

// Press each button at a time and make sure it goes to ACTIVATED state,
// followed by HIDDEN state after selecting other button.
TEST_F(IntentPickerBubbleViewTest, InkDropStateTransition) {
  CreateBubbleView(true);
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);
  size_t size = bubble_->app_info_.size();
  for (size_t i = 0; i < size; ++i) {
    bubble_->PressButtonForTesting((i + 1) % size, event);
    EXPECT_EQ(bubble_->GetInkDropStateForTesting(i),
              views::InkDropState::HIDDEN);
    EXPECT_EQ(bubble_->GetInkDropStateForTesting((i + 1) % size),
              views::InkDropState::ACTIVATED);
  }
}

// Arbitrary press the first button twice, check that the InkDropState remains
// the same.
TEST_F(IntentPickerBubbleViewTest, PressButtonTwice) {
  CreateBubbleView(true);
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);
  EXPECT_EQ(bubble_->GetInkDropStateForTesting(0), views::InkDropState::HIDDEN);
  bubble_->PressButtonForTesting(0, event);
  EXPECT_EQ(bubble_->GetInkDropStateForTesting(0),
            views::InkDropState::ACTIVATED);
  bubble_->PressButtonForTesting(0, event);
  EXPECT_EQ(bubble_->GetInkDropStateForTesting(0),
            views::InkDropState::ACTIVATED);
}

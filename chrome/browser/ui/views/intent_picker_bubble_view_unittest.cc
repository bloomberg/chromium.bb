// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/intent_picker_bubble_view.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/arc/arc_navigation_throttle.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/resources/grit/views_resources.h"
#include "url/gurl.h"

using NameAndIcon = arc::ArcNavigationThrottle::NameAndIcon;
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
    app_info_.emplace_back("dank app 1", gfx::Image());
    app_info_.emplace_back("dank app 2", gfx::Image());

    if (use_icons)
      FillAppListWithDummyIcons();

    // We create |web_contents| since the Bubble UI has an Observer that
    // depends on this, otherwise it wouldn't work.
    GURL url("http://www.google.com");
    WebContents* web_contents = browser()->OpenURL(OpenURLParams(
        url, Referrer(), CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));

    bubble_ = IntentPickerBubbleView::CreateBubbleView(
        app_info_, base::Bind(&IntentPickerBubbleViewTest::OnBubbleClosed,
                              base::Unretained(this)),
        web_contents);
  }

  void FillAppListWithDummyIcons() {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    gfx::Image dummy_icon = rb.GetImageNamed(IDR_CLOSE);
    for (auto& app : app_info_)
      app.second = dummy_icon;
  }

  // Dummy method to be called upon bubble closing.
  void OnBubbleClosed(size_t selected_app_tag,
                      arc::ArcNavigationThrottle::CloseReason close_reason) {}

  std::unique_ptr<IntentPickerBubbleView> bubble_;
  std::vector<NameAndIcon> app_info_;

 private:
  DISALLOW_COPY_AND_ASSIGN(IntentPickerBubbleViewTest);
};

// Verifies that we didn't set up an image for any LabelButton.
TEST_F(IntentPickerBubbleViewTest, NullIcons) {
  CreateBubbleView(false);
  size_t size = bubble_->app_info_.size();
  for (size_t i = 0; i < size; ++i) {
    views::LabelButton* app = bubble_->GetLabelButtonAt(i);
    EXPECT_TRUE(
        app->GetImage(views::Button::ButtonState::STATE_NORMAL).isNull()) << i;
  }
}

// Verifies that all the icons contain a non-null icon.
TEST_F(IntentPickerBubbleViewTest, NonNullIcons) {
  CreateBubbleView(true);
  size_t size = bubble_->app_info_.size();
  for (size_t i = 0; i < size; ++i) {
    views::LabelButton* app = bubble_->GetLabelButtonAt(i);
    EXPECT_FALSE(
        app->GetImage(views::Button::ButtonState::STATE_NORMAL).isNull()) << i;
  }
}

// Verifies that the bubble contains as many rows as the input. Populated the
// bubble with an arbitrary image in every row.
TEST_F(IntentPickerBubbleViewTest, LabelsPtrVectorSize) {
  CreateBubbleView(true);
  EXPECT_EQ(app_info_.size(), bubble_->app_info_.size());
}

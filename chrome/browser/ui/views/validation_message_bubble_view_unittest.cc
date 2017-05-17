// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/validation_message_bubble_view.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_switches.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/test/scoped_views_test_helper.h"
#include "ui/views/test/test_views_delegate.h"

namespace {
constexpr float kScaleFactor = 1.5;
constexpr gfx::Rect kInitialAnchorRect = gfx::Rect(10, 20, 30, 40);
}  // namespace

class ValidationMessageBubbleViewTest : public ChromeRenderViewHostTestHarness {
 public:
  ValidationMessageBubbleViewTest() = default;

  void SetUp() override {
    // Append the switch before any other setup.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kForceDeviceScaleFactor, std::to_string(kScaleFactor));

    ChromeRenderViewHostTestHarness::SetUp();

    bubble_ = new ValidationMessageBubbleView(
        web_contents(), kInitialAnchorRect, base::ASCIIToUTF16("MAIN TEXT"),
        base::ASCIIToUTF16("SUB TEXT"));
  }

  ValidationMessageBubbleView& bubble() { return *bubble_; }

 private:
  views::LayoutProvider provider_;  // Creates a singleton.
  ValidationMessageBubbleView* bubble_;

  DISALLOW_COPY_AND_ASSIGN(ValidationMessageBubbleViewTest);
};

TEST_F(ValidationMessageBubbleViewTest,
       AnchorRectIsCorrectForDeviceScaleFactor) {
  constexpr float inverse_scale_factor = 1 / kScaleFactor;
  EXPECT_EQ(gfx::ScaleToEnclosingRect(kInitialAnchorRect, inverse_scale_factor),
            bubble().anchor_rect());

  const gfx::Rect updated_anchor_rect(50, 60, 70, 80);
  bubble().SetPositionRelativeToAnchor(
      web_contents()->GetRenderWidgetHostView()->GetRenderWidgetHost(),
      updated_anchor_rect);
  EXPECT_EQ(
      gfx::ScaleToEnclosingRect(updated_anchor_rect, inverse_scale_factor),
      bubble().anchor_rect());
}

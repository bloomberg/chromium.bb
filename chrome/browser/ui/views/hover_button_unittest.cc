// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/hover_button.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

namespace {

constexpr int kButtonWidth = 150;

struct TitleSubtitlePair {
  const char* const title;
  const char* const subtitle;
  // Whether the HoverButton is expected to have a tooltip for this text.
  bool tooltip;
};

constexpr TitleSubtitlePair kTitleSubtitlePairs[] = {
    // Two short strings that will fit in the space given.
    {"Clap!", "Clap!", false},
    // First string fits, second string doesn't.
    {"If you're happy and you know it, clap your hands!", "Clap clap!", true},
    // Second string fits, first string doesn't.
    {"Clap clap!",
     "If you're happy and you know it, and you really want to show it,", true},
    // Both strings don't fit.
    {"If you're happy and you know it, and you really want to show it,",
     "If you're happy and you know it, clap your hands!", true},
};

class HoverButtonTest : public views::ViewsTestBase {
 public:
  HoverButtonTest() {}

  std::unique_ptr<views::View> CreateIcon() {
    auto icon = std::make_unique<views::View>();
    icon->SetPreferredSize(gfx::Size(16, 16));
    return icon;
  }

  // views::ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();
    // HoverButton uses Chrome-specific layout constants, so make sure these
    // exist for testing.
    test_views_delegate()->set_layout_provider(
        ChromeLayoutProvider::CreateLayoutProvider());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HoverButtonTest);
};

}  // namespace

// Double check the length of the strings used for testing are either over or
// under the width used for the following tests.
TEST_F(HoverButtonTest, ValidateTestData) {
  auto get_width = [](const char* text) {
    return views::Label(base::ASCIIToUTF16(text)).GetPreferredSize().width();
  };
  EXPECT_GT(kButtonWidth, get_width(kTitleSubtitlePairs[0].title));
  EXPECT_GT(kButtonWidth, get_width(kTitleSubtitlePairs[0].subtitle));

  EXPECT_LT(kButtonWidth, get_width(kTitleSubtitlePairs[1].title));
  EXPECT_GT(kButtonWidth, get_width(kTitleSubtitlePairs[1].subtitle));

  EXPECT_GT(kButtonWidth, get_width(kTitleSubtitlePairs[2].title));
  EXPECT_LT(kButtonWidth, get_width(kTitleSubtitlePairs[2].subtitle));

  EXPECT_LT(kButtonWidth, get_width(kTitleSubtitlePairs[3].title));
  EXPECT_LT(kButtonWidth, get_width(kTitleSubtitlePairs[3].subtitle));
}

// Tests whether the HoverButton has the correct tooltip and accessible name.
TEST_F(HoverButtonTest, TooltipAndAccessibleName) {
  for (size_t i = 0; i < arraysize(kTitleSubtitlePairs); ++i) {
    TitleSubtitlePair pair = kTitleSubtitlePairs[i];
    SCOPED_TRACE(testing::Message() << "Index: " << i << ", expected_tooltip="
                                    << (pair.tooltip ? "true" : "false"));
    auto button = std::make_unique<HoverButton>(
        nullptr, CreateIcon(), base::ASCIIToUTF16(pair.title),
        base::ASCIIToUTF16(pair.subtitle));
    button->SetSize(gfx::Size(kButtonWidth, 40));

    ui::AXNodeData data;
    button->GetAccessibleNodeData(&data);
    std::string accessible_name;
    data.GetStringAttribute(ui::AX_ATTR_NAME, &accessible_name);

    // The accessible name should always be the title and subtitle concatenated
    // by \n.
    base::string16 expected = base::JoinString(
        {base::ASCIIToUTF16(pair.title), base::ASCIIToUTF16(pair.subtitle)},
        base::ASCIIToUTF16("\n"));
    EXPECT_EQ(expected, base::UTF8ToUTF16(accessible_name));

    base::string16 tooltip_text;
    button->GetTooltipText(gfx::Point(), &tooltip_text);
    if (pair.tooltip) {
      EXPECT_EQ(expected, tooltip_text);
    } else {
      EXPECT_EQ(base::string16(), tooltip_text);
    }
  }
}

// Tests that setting a custom tooltip on a HoverButton will not be overwritten
// by HoverButton's own tooltips.
TEST_F(HoverButtonTest, CustomTooltip) {
  const base::string16 custom_tooltip = base::ASCIIToUTF16("custom");

  for (size_t i = 0; i < arraysize(kTitleSubtitlePairs); ++i) {
    SCOPED_TRACE(testing::Message() << "Index: " << i);
    TitleSubtitlePair pair = kTitleSubtitlePairs[i];
    auto button = std::make_unique<HoverButton>(
        nullptr, CreateIcon(), base::ASCIIToUTF16(pair.title),
        base::ASCIIToUTF16(pair.subtitle));
    button->set_auto_compute_tooltip(false);
    button->SetTooltipText(custom_tooltip);
    button->SetSize(gfx::Size(kButtonWidth, 40));

    base::string16 tooltip_text;
    button->GetTooltipText(gfx::Point(), &tooltip_text);
    EXPECT_EQ(custom_tooltip, tooltip_text);

    // Make sure the accessible name is still set.
    ui::AXNodeData data;
    button->GetAccessibleNodeData(&data);
    std::string accessible_name;
    data.GetStringAttribute(ui::AX_ATTR_NAME, &accessible_name);

    // The accessible name should always be the title and subtitle concatenated
    // by \n.
    base::string16 expected = base::JoinString(
        {base::ASCIIToUTF16(pair.title), base::ASCIIToUTF16(pair.subtitle)},
        base::ASCIIToUTF16("\n"));
    EXPECT_EQ(expected, base::UTF8ToUTF16(accessible_name));
  }
}

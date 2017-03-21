// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_views_delegate.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/default_style.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font_list.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

// Check legacy font sizes. No new code should be using these constants, but if
// these tests ever fail it probably means something in the old UI will have
// changed by mistake.
// Disabled since this relies on machine configuration. http://crbug.com/701241.
TEST(LayoutDelegateTest, DISABLED_LegacyFontSizeConstants) {
  ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::FontList label_font = rb.GetFontListWithDelta(ui::kLabelFontSizeDelta);

  EXPECT_EQ(12, label_font.GetFontSize());
  EXPECT_EQ(15, label_font.GetHeight());
  EXPECT_EQ(12, label_font.GetBaseline());
  EXPECT_EQ(9, label_font.GetCapHeight());
// Note some Windows bots report 11,13,11,9 for the above.
// TODO(tapted): Smoke them out and figure out why.

#if defined(OS_MACOSX)
  if (base::mac::IsOS10_9()) {
    EXPECT_EQ(6, label_font.GetExpectedTextWidth(1));
  } else {
    EXPECT_EQ(10, label_font.GetExpectedTextWidth(1));
  }
#else
  EXPECT_EQ(6, label_font.GetExpectedTextWidth(1));
// Some Windows bots may say 5.
#endif

  gfx::FontList title_font = rb.GetFontListWithDelta(ui::kTitleFontSizeDelta);

#if defined(OS_WIN)
  EXPECT_EQ(15, title_font.GetFontSize());
  EXPECT_EQ(20, title_font.GetHeight());
  EXPECT_EQ(16, title_font.GetBaseline());
  EXPECT_EQ(11, title_font.GetCapHeight());
#elif defined(OS_MACOSX)
  EXPECT_EQ(14, title_font.GetFontSize());
  EXPECT_EQ(17, title_font.GetHeight());
  EXPECT_EQ(14, title_font.GetBaseline());
  if (base::mac::IsOS10_9()) {
    EXPECT_EQ(11, title_font.GetCapHeight());
  } else {
    EXPECT_EQ(10, title_font.GetCapHeight());
  }
#else
  EXPECT_EQ(15, title_font.GetFontSize());
  EXPECT_EQ(18, title_font.GetHeight());
  EXPECT_EQ(14, title_font.GetBaseline());
  EXPECT_EQ(11, title_font.GetCapHeight());
#endif

#if defined(OS_MACOSX)
  if (base::mac::IsOS10_9()) {
    EXPECT_EQ(7, title_font.GetExpectedTextWidth(1));
  } else {
    EXPECT_EQ(12, title_font.GetExpectedTextWidth(1));
  }
#else
  EXPECT_EQ(8, title_font.GetExpectedTextWidth(1));
#endif

  gfx::FontList small_font = rb.GetFontList(ResourceBundle::SmallFont);
  gfx::FontList base_font = rb.GetFontList(ResourceBundle::BaseFont);
  gfx::FontList bold_font = rb.GetFontList(ResourceBundle::BoldFont);
  gfx::FontList medium_font = rb.GetFontList(ResourceBundle::MediumFont);
  gfx::FontList medium_bold_font =
      rb.GetFontList(ResourceBundle::MediumBoldFont);
  gfx::FontList large_font = rb.GetFontList(ResourceBundle::LargeFont);

#if defined(OS_MACOSX)
  EXPECT_EQ(12, small_font.GetFontSize());
  EXPECT_EQ(13, base_font.GetFontSize());
  EXPECT_EQ(13, bold_font.GetFontSize());
  EXPECT_EQ(16, medium_font.GetFontSize());
  EXPECT_EQ(16, medium_bold_font.GetFontSize());
  EXPECT_EQ(21, large_font.GetFontSize());
#else
  EXPECT_EQ(11, small_font.GetFontSize());
  EXPECT_EQ(12, base_font.GetFontSize());
  EXPECT_EQ(12, bold_font.GetFontSize());
  EXPECT_EQ(15, medium_font.GetFontSize());
  EXPECT_EQ(15, medium_bold_font.GetFontSize());
  EXPECT_EQ(20, large_font.GetFontSize());
#endif
}

// Check that asking for fonts of a given size match the Harmony spec. If these
// tests fail, the Harmony TypographyProvider needs to be updated to handle the
// new font properties. For example, when title_font.GetHeight() returns 19, the
// Harmony TypographyProvider adds 3 to obtain its target height of 22. If a
// platform starts returning 18 in a standard configuration then the
// TypographyProvider must add 4 instead. We do this so that Chrome adapts
// correctly to _non-standard_ system font configurations on user machines.
// Disabled since this relies on machine configuration. http://crbug.com/701241.
TEST(LayoutDelegateTest, DISABLED_RequestFontBySize) {
#if defined(OS_MACOSX)
  constexpr int kBase = 13;
#else
  constexpr int kBase = 12;
#endif
  // Harmony spec.
  constexpr int kHeadline = 20;
  constexpr int kTitle = 15;  // Leading 22.
  constexpr int kBody1 = 13;  // Leading 20.
  constexpr int kBody2 = 12;  // Leading 20.
  constexpr int kButton = 12;

#if defined(OS_WIN)
  constexpr gfx::Font::Weight kButtonWeight = gfx::Font::Weight::BOLD;
#else
  constexpr gfx::Font::Weight kButtonWeight = gfx::Font::Weight::MEDIUM;
#endif

  ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  gfx::FontList headline_font = rb.GetFontListWithDelta(kHeadline - kBase);
  gfx::FontList title_font = rb.GetFontListWithDelta(kTitle - kBase);
  gfx::FontList body1_font = rb.GetFontListWithDelta(kBody1 - kBase);
  gfx::FontList body2_font = rb.GetFontListWithDelta(kBody2 - kBase);
  gfx::FontList button_font = rb.GetFontListWithDelta(
      kButton - kBase, gfx::Font::NORMAL, kButtonWeight);

  // The following checks on leading don't need to match the spec. Instead, it
  // means Label::SetLineHeight() needs to be used to increase it. But what we
  // are really interested in is the delta between GetFontSize() and GetHeight()
  // since that (plus a fixed constant) determines how the leading should change
  // when a larger font is configured in the OS.

  EXPECT_EQ(kHeadline, headline_font.GetFontSize());

// Headline leading not specified (multiline should be rare).
#if defined(OS_MACOSX)
  EXPECT_EQ(25, headline_font.GetHeight());
#elif defined(OS_WIN)
  EXPECT_EQ(28, headline_font.GetHeight());
#else
  EXPECT_EQ(24, headline_font.GetHeight());
#endif

  EXPECT_EQ(kTitle, title_font.GetFontSize());

// Title font leading should be 22.
#if defined(OS_MACOSX)
  EXPECT_EQ(19, title_font.GetHeight());  // i.e. Add 3 to obtain line height.
#elif defined(OS_WIN)
  EXPECT_EQ(20, title_font.GetHeight());  // Add 2.
#else
  EXPECT_EQ(18, title_font.GetHeight());  // Add 4.
#endif

  EXPECT_EQ(kBody1, body1_font.GetFontSize());

// Body1 font leading should be 20.
#if defined(OS_MACOSX)
  EXPECT_EQ(16, body1_font.GetHeight());  // Add 4.
#else  // Win and Linux.
  EXPECT_EQ(17, body1_font.GetHeight());  // Add 3.
#endif

  EXPECT_EQ(kBody2, body2_font.GetFontSize());

  // Body2 font leading should be 20.
  EXPECT_EQ(15, body2_font.GetHeight());  // All platforms: Add 5.

  EXPECT_EQ(kButton, button_font.GetFontSize());

  // Button leading not specified (shouldn't be needed: no multiline buttons).
  EXPECT_EQ(15, button_font.GetHeight());
}

// Test that the default TypographyProvider correctly maps TextContexts relative
// to the "base" font in the manner that legacy toolkit-views code expects. This
// reads the base font configuration at runtime, and only tests font sizes, so
// should be robust against platform changes.
TEST(LayoutDelegateTest, FontSizeRelativeToBase) {
  using views::style::GetFont;

  constexpr int kStyle = views::style::STYLE_PRIMARY;

  // Typography described in chrome_typography.h requires a ChromeViewsDelegate.
  ChromeViewsDelegate views_delegate;

// Legacy code measures everything relative to a default-constructed FontList.
// On Mac, subtract one since that is 13pt instead of 12pt.
#if defined(OS_MACOSX)
  const int twelve = gfx::FontList().GetFontSize() - 1;
#else
  const int twelve = gfx::FontList().GetFontSize();
#endif

  EXPECT_EQ(twelve, GetFont(CONTEXT_BODY_TEXT_SMALL, kStyle).GetFontSize());
  EXPECT_EQ(twelve, GetFont(views::style::CONTEXT_LABEL, kStyle).GetFontSize());
  EXPECT_EQ(twelve,
            GetFont(views::style::CONTEXT_TEXTFIELD, kStyle).GetFontSize());
  EXPECT_EQ(twelve,
            GetFont(views::style::CONTEXT_BUTTON, kStyle).GetFontSize());

#if defined(OS_MACOSX)
  // We never exposed UI on Mac using these constants so it doesn't matter that
  // they are different. They only need to match under Harmony.
  EXPECT_EQ(twelve + 9, GetFont(CONTEXT_HEADLINE, kStyle).GetFontSize());
  EXPECT_EQ(twelve + 2,
            GetFont(views::style::CONTEXT_DIALOG_TITLE, kStyle).GetFontSize());
  EXPECT_EQ(twelve + 2, GetFont(CONTEXT_BODY_TEXT_LARGE, kStyle).GetFontSize());
  EXPECT_EQ(twelve, GetFont(CONTEXT_DEPRECATED_SMALL, kStyle).GetFontSize());
#else
  // E.g. Headline should give a 20pt font.
  EXPECT_EQ(twelve + 8, GetFont(CONTEXT_HEADLINE, kStyle).GetFontSize());
  // Titles should be 15pt. Etc.
  EXPECT_EQ(twelve + 3,
            GetFont(views::style::CONTEXT_DIALOG_TITLE, kStyle).GetFontSize());
  EXPECT_EQ(twelve + 1, GetFont(CONTEXT_BODY_TEXT_LARGE, kStyle).GetFontSize());
  EXPECT_EQ(twelve - 1,
            GetFont(CONTEXT_DEPRECATED_SMALL, kStyle).GetFontSize());
#endif
}

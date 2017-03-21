// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HARMONY_CHROME_TYPOGRAPHY_H_
#define CHROME_BROWSER_UI_VIEWS_HARMONY_CHROME_TYPOGRAPHY_H_

#include "base/macros.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"

enum ChromeTextContext {
  // Headline text. Usually 20pt. Never multi-line.
  CONTEXT_HEADLINE = views::style::VIEWS_TEXT_CONTEXT_END,

  // "Body 1". Usually 13pt.
  CONTEXT_BODY_TEXT_LARGE,

  // "Body 2". Usually 12pt.
  CONTEXT_BODY_TEXT_SMALL,

  // ResourceBundle::SmallFont (11 pt). There is no equivalent in the Harmony
  // spec, so new code should not be using this. It is only provided to avoid
  // changing existing UI and it will eventually be removed.
  CONTEXT_DEPRECATED_SMALL,
};

enum ChromeTextStyle {
  // Secondary text. May be lighter than views::style::PRIMARY.
  STYLE_SECONDARY = views::style::VIEWS_TEXT_STYLE_END,

  // "Hint" text, usually a line that gives context to something more important.
  STYLE_HINT,

  // A solid shade of red.
  STYLE_RED,

  // A solid shade of green.
  STYLE_GREEN,

  // Used to draw attention to a section of body text such as an extension name
  // or hostname.
  STYLE_EMPHASIZED,
};

// TypographyProvider that provides pre-Harmony fonts in Chrome.
class LegacyTypographyProvider : public views::DefaultTypographyProvider {
 public:
  LegacyTypographyProvider() = default;

  // DefaultTypographyProvider:
  const gfx::FontList& GetFont(int text_context, int text_style) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LegacyTypographyProvider);
};

#endif  // CHROME_BROWSER_UI_VIEWS_HARMONY_CHROME_TYPOGRAPHY_H_

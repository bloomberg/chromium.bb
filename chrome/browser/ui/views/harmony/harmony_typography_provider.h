// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HARMONY_HARMONY_TYPOGRAPHY_PROVIDER_H_
#define CHROME_BROWSER_UI_VIEWS_HARMONY_HARMONY_TYPOGRAPHY_PROVIDER_H_

#include "base/macros.h"
#include "ui/views/style/typography_provider.h"

// TypographyProvider implementing the Harmony spec.
class HarmonyTypographyProvider : public views::TypographyProvider {
 public:
  HarmonyTypographyProvider() = default;

  // TypographyProvider:
  const gfx::FontList& GetFont(int text_context, int text_style) const override;
  SkColor GetColor(int context, int style) const override;
  int GetLineHeight(int context, int style) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(HarmonyTypographyProvider);
};

#endif  // CHROME_BROWSER_UI_VIEWS_HARMONY_HARMONY_TYPOGRAPHY_PROVIDER_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/feature_promos/new_tab_promo_bubble_view.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "components/variations/variations_associated_data.h"

// static
NewTabPromoBubbleView* NewTabPromoBubbleView::CreateOwned(
    const gfx::Rect& anchor_rect) {
  return new NewTabPromoBubbleView(anchor_rect);
}

NewTabPromoBubbleView::NewTabPromoBubbleView(const gfx::Rect& anchor_rect)
    : FeaturePromoBubbleView(anchor_rect,
                             views::BubbleBorder::LEFT_TOP,
                             GetStringSpecifier()) {}

NewTabPromoBubbleView::~NewTabPromoBubbleView() = default;

// static
int NewTabPromoBubbleView::GetStringSpecifier() {
  static constexpr int kTextIds[] = {IDS_NEWTAB_PROMO_0, IDS_NEWTAB_PROMO_1,
                                     IDS_NEWTAB_PROMO_2};
  const std::string& str = variations::GetVariationParamValue(
      "NewTabInProductHelp", "x_promo_string");
  size_t text_specifier;
  if (!base::StringToSizeT(str, &text_specifier) ||
      text_specifier >= arraysize(kTextIds)) {
    text_specifier = 0;
  }

  return kTextIds[text_specifier];
}

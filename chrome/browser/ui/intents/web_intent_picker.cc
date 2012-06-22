// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intents/web_intent_picker.h"

#include <cmath>

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/theme_resources_standard.h"
#include "ui/gfx/size.h"

namespace {

// The minimum width of the inline disposition tab contents.
const int kMinInlineDispositionWidth = 300;

// The minimum height of the inline disposition tab contents.
const int kMinInlineDispositionHeight = 150;

// Maximum inline disposition container sizes.
const int kMaxInlineDispositionWidth = 900;
const int kMaxInlineDispositionHeight = 900;

}  // namespace

// static
gfx::Size WebIntentPicker::GetDefaultInlineDispositionSize(
    content::WebContents* web_contents) {
  // TODO(gbillock): This size calculation needs more thought.
  gfx::Size tab_size = web_contents->GetView()->GetContainerSize();
  int width = std::max(tab_size.width()/2, kMinInlineDispositionWidth);
  int height = std::max(tab_size.height()/2, kMinInlineDispositionHeight);

  return gfx::Size(width, height);
}

// static
gfx::Size WebIntentPicker::GetMinInlineDispositionSize() {
  return gfx::Size(kMinInlineDispositionWidth, kMinInlineDispositionHeight);
}

// static
gfx::Size WebIntentPicker::GetMaxInlineDispositionSize() {
  return gfx::Size(kMaxInlineDispositionWidth, kMaxInlineDispositionHeight);
}

// static
int WebIntentPicker::GetNthStarImageIdFromCWSRating(double rating, int index) {
  // The fractional part of the rating is converted to stars as:
  //   [0, 0.33) -> round down
  //   [0.33, 0.66] -> half star
  //   (0.66, 1) -> round up
  const double kHalfStarMin = 0.33;
  const double kHalfStarMax = 0.66;

  // Add 1 + kHalfStarMax to make values with fraction > 0.66 round up.
  int full_stars = std::floor(rating + 1 - kHalfStarMax);

  if (index < full_stars)
    return IDR_CWS_STAR_FULL;

  // We don't need to test against the upper bound (kHalfStarMax); when the
  // fractional part is greater than kHalfStarMax, full_stars will be rounded
  // up, which means rating - full_stars is negative.
  bool half_star = rating - full_stars >= kHalfStarMin;

  return index == full_stars && half_star ?
      IDR_CWS_STAR_HALF :
      IDR_CWS_STAR_EMPTY;
}

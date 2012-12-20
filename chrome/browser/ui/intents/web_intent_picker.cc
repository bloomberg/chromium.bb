// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intents/web_intent_picker.h"

#include <cmath>

#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/intents/web_intents_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/size.h"

namespace {

// Maximum inline disposition container sizes.
const int kMaxInlineDispositionWidth = 900;
const int kMaxInlineDispositionHeight = 900;

}  // namespace

void WebIntentPicker::OnShowExtensionInstallDialog(
    const ExtensionInstallPrompt::ShowParams& show_params,
    ExtensionInstallPrompt::Delegate* delegate,
    const ExtensionInstallPrompt::Prompt& prompt) {
  ExtensionInstallPrompt::GetDefaultShowDialogCallback().Run(
      show_params, delegate, prompt);
}

gfx::Size WebIntentPicker::GetMinInlineDispositionSize() {
  return gfx::Size(1, 1);
}

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

// static
string16 WebIntentPicker::GetDisplayStringForIntentAction(
    const string16& action16) {
  std::string action(UTF16ToUTF8(action16));
  if (!action.compare(web_intents::kActionShare))
    return l10n_util::GetStringUTF16(IDS_WEB_INTENTS_ACTION_SHARE);
  else if (!action.compare(web_intents::kActionEdit))
    return l10n_util::GetStringUTF16(IDS_WEB_INTENTS_ACTION_EDIT);
  else if (!action.compare(web_intents::kActionView))
    return l10n_util::GetStringUTF16(IDS_WEB_INTENTS_ACTION_VIEW);
  else if (!action.compare(web_intents::kActionPick))
    // Using generic string per UX suggestions.
    return l10n_util::GetStringUTF16(IDS_INTENT_PICKER_CHOOSE_SERVICE);
  else if (!action.compare(web_intents::kActionSubscribe))
    return l10n_util::GetStringUTF16(IDS_WEB_INTENTS_ACTION_SUBSCRIBE);
  else if (!action.compare(web_intents::kActionSave))
    return l10n_util::GetStringUTF16(IDS_WEB_INTENTS_ACTION_SAVE);
  else
    return l10n_util::GetStringUTF16(IDS_INTENT_PICKER_CHOOSE_SERVICE);
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_types.h"

#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace autofill {

const int kSplashDisplayDurationMs = 1200;
const int kSplashFadeOutDurationMs = 200;
const int kSplashFadeInDialogDurationMs = 150;

static const base::char16 kRangeSeparator = '|';

DialogNotification::DialogNotification() : type_(NONE) {}

DialogNotification::DialogNotification(Type type, const string16& display_text)
    : type_(type),
      display_text_(display_text),
      checked_(false),
      interactive_(true) {
  // If there's a range separated by bars, mark that as the anchor text.
  std::vector<base::string16> pieces;
  base::SplitStringDontTrim(display_text, kRangeSeparator, &pieces);
  if (pieces.size() > 1) {
    link_range_ = ui::Range(pieces[0].size(), pieces[1].size());
    display_text_ = JoinString(pieces, string16());
  }
}

DialogNotification::~DialogNotification() {}

SkColor DialogNotification::GetBackgroundColor() const {
  switch (type_) {
    case DialogNotification::AUTOCHECKOUT_SUCCESS:
    case DialogNotification::EXPLANATORY_MESSAGE:
    case DialogNotification::WALLET_USAGE_CONFIRMATION:
      return SkColorSetRGB(0xf5, 0xf5, 0xf5);
    case DialogNotification::REQUIRED_ACTION:
    case DialogNotification::WALLET_ERROR:
    case DialogNotification::AUTOCHECKOUT_ERROR:
      return SkColorSetRGB(0xfc, 0xf3, 0xbf);
    case DialogNotification::DEVELOPER_WARNING:
    case DialogNotification::SECURITY_WARNING:
    case DialogNotification::VALIDATION_ERROR:
      return kWarningColor;
    case DialogNotification::NONE:
      return SK_ColorTRANSPARENT;
  }

  NOTREACHED();
  return SK_ColorTRANSPARENT;
}

SkColor DialogNotification::GetBorderColor() const {
  switch (type_) {
    case DialogNotification::AUTOCHECKOUT_SUCCESS:
    case DialogNotification::EXPLANATORY_MESSAGE:
    case DialogNotification::WALLET_USAGE_CONFIRMATION:
      return SkColorSetRGB(0xe5, 0xe5, 0xe5);
    case DialogNotification::REQUIRED_ACTION:
    case DialogNotification::WALLET_ERROR:
    case DialogNotification::AUTOCHECKOUT_ERROR:
    case DialogNotification::DEVELOPER_WARNING:
    case DialogNotification::SECURITY_WARNING:
    case DialogNotification::VALIDATION_ERROR:
    case DialogNotification::NONE:
      return GetBackgroundColor();
  }

  NOTREACHED();
  return SK_ColorTRANSPARENT;
}

SkColor DialogNotification::GetTextColor() const {
  switch (type_) {
    case DialogNotification::REQUIRED_ACTION:
    case DialogNotification::WALLET_ERROR:
    case DialogNotification::AUTOCHECKOUT_ERROR:
    case DialogNotification::AUTOCHECKOUT_SUCCESS:
    case DialogNotification::EXPLANATORY_MESSAGE:
    case DialogNotification::WALLET_USAGE_CONFIRMATION:
      return SkColorSetRGB(102, 102, 102);
    case DialogNotification::DEVELOPER_WARNING:
    case DialogNotification::SECURITY_WARNING:
    case DialogNotification::VALIDATION_ERROR:
      return SK_ColorWHITE;
    case DialogNotification::NONE:
      return SK_ColorTRANSPARENT;
  }

  NOTREACHED();
  return SK_ColorTRANSPARENT;
}

bool DialogNotification::HasArrow() const {
  return type_ == DialogNotification::EXPLANATORY_MESSAGE ||
         type_ == DialogNotification::WALLET_ERROR ||
         type_ == DialogNotification::WALLET_USAGE_CONFIRMATION;
}

bool DialogNotification::HasCheckbox() const {
  return type_ == DialogNotification::WALLET_USAGE_CONFIRMATION;
}

DialogAutocheckoutStep::DialogAutocheckoutStep(AutocheckoutStepType type,
                                               AutocheckoutStepStatus status)
    : type_(type),
      status_(status) {}

SkColor DialogAutocheckoutStep::GetTextColor() const {
  switch (status_) {
    case AUTOCHECKOUT_STEP_UNSTARTED:
      return SK_ColorGRAY;

    case AUTOCHECKOUT_STEP_STARTED:
    case AUTOCHECKOUT_STEP_COMPLETED:
      return SK_ColorBLACK;

    case AUTOCHECKOUT_STEP_FAILED:
      return SK_ColorRED;
  }

  NOTREACHED();
  return SK_ColorTRANSPARENT;
}

gfx::Font DialogAutocheckoutStep::GetTextFont() const {
  gfx::Font::FontStyle font_style = gfx::Font::NORMAL;
  switch (status_) {
    case AUTOCHECKOUT_STEP_UNSTARTED:
    case AUTOCHECKOUT_STEP_STARTED:
      font_style = gfx::Font::NORMAL;
      break;

    case AUTOCHECKOUT_STEP_COMPLETED:
    case AUTOCHECKOUT_STEP_FAILED:
      font_style = gfx::Font::BOLD;
      break;
  }

  return ui::ResourceBundle::GetSharedInstance().GetFont(
      ui::ResourceBundle::BaseFont).DeriveFont(0, font_style);
}

bool DialogAutocheckoutStep::IsIconVisible() const {
  return status_ == AUTOCHECKOUT_STEP_COMPLETED;
}

string16 DialogAutocheckoutStep::GetDisplayText() const {
  int description_id = -1;
  switch (status_) {
    case AUTOCHECKOUT_STEP_UNSTARTED:
      switch (type_) {
        case AUTOCHECKOUT_STEP_SHIPPING:
          description_id = IDS_AUTOFILL_STEP_SHIPPING_DETAILS_UNSTARTED;
          break;

        case AUTOCHECKOUT_STEP_DELIVERY:
          description_id = IDS_AUTOFILL_STEP_DELIVERY_DETAILS_UNSTARTED;
          break;

        case AUTOCHECKOUT_STEP_BILLING:
          description_id = IDS_AUTOFILL_STEP_BILLING_DETAILS_UNSTARTED;
          break;

        case AUTOCHECKOUT_STEP_PROXY_CARD:
          description_id = IDS_AUTOFILL_STEP_PROXY_CARD_UNSTARTED;
          break;
      }
      break;

    case AUTOCHECKOUT_STEP_STARTED:
      switch (type_) {
        case AUTOCHECKOUT_STEP_SHIPPING:
          description_id = IDS_AUTOFILL_STEP_SHIPPING_DETAILS_STARTED;
          break;

        case AUTOCHECKOUT_STEP_DELIVERY:
          description_id = IDS_AUTOFILL_STEP_DELIVERY_DETAILS_STARTED;
          break;

        case AUTOCHECKOUT_STEP_BILLING:
          description_id = IDS_AUTOFILL_STEP_BILLING_DETAILS_STARTED;
          break;

        case AUTOCHECKOUT_STEP_PROXY_CARD:
          description_id = IDS_AUTOFILL_STEP_PROXY_CARD_STARTED;
          break;
      }
      break;

    case AUTOCHECKOUT_STEP_COMPLETED:
      switch (type_) {
        case AUTOCHECKOUT_STEP_SHIPPING:
          description_id = IDS_AUTOFILL_STEP_SHIPPING_DETAILS_COMPLETE;
          break;

        case AUTOCHECKOUT_STEP_DELIVERY:
          description_id = IDS_AUTOFILL_STEP_DELIVERY_DETAILS_COMPLETE;
          break;

        case AUTOCHECKOUT_STEP_BILLING:
          description_id = IDS_AUTOFILL_STEP_BILLING_DETAILS_COMPLETE;
          break;

        case AUTOCHECKOUT_STEP_PROXY_CARD:
          description_id = IDS_AUTOFILL_STEP_PROXY_CARD_COMPLETE;
          break;
      }
      break;

    case AUTOCHECKOUT_STEP_FAILED:
      switch (type_) {
        case AUTOCHECKOUT_STEP_SHIPPING:
          description_id = IDS_AUTOFILL_STEP_SHIPPING_DETAILS_FAILED;
          break;

        case AUTOCHECKOUT_STEP_DELIVERY:
          description_id = IDS_AUTOFILL_STEP_DELIVERY_DETAILS_FAILED;
          break;

        case AUTOCHECKOUT_STEP_BILLING:
          description_id = IDS_AUTOFILL_STEP_BILLING_DETAILS_FAILED;
          break;

        case AUTOCHECKOUT_STEP_PROXY_CARD:
          description_id = IDS_AUTOFILL_STEP_PROXY_CARD_FAILED;
          break;
      }
      break;
  }

  return l10n_util::GetStringUTF16(description_id);
}

SkColor const kWarningColor = SkColorSetRGB(0xde, 0x49, 0x32);

SuggestionState::SuggestionState()
    : visible(false) {}
SuggestionState::SuggestionState(bool visible,
                                 const string16& vertically_compact_text,
                                 const string16& horizontally_compact_text,
                                 const gfx::Image& icon,
                                 const string16& extra_text,
                                 const gfx::Image& extra_icon)
    : visible(visible),
      vertically_compact_text(vertically_compact_text),
      horizontally_compact_text(horizontally_compact_text),
      icon(icon),
      extra_text(extra_text),
      extra_icon(extra_icon) {}
SuggestionState::~SuggestionState() {}

DialogOverlayString::DialogOverlayString() : alignment(gfx::ALIGN_LEFT) {}
DialogOverlayString::~DialogOverlayString() {}

DialogOverlayState::DialogOverlayState() {}
DialogOverlayState::~DialogOverlayState() {}

}  // namespace autofill

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_types.h"

#include "base/logging.h"

namespace autofill {

DialogNotification::DialogNotification() : type_(NONE) {}

DialogNotification::DialogNotification(Type type, const string16& display_text)
    : type_(type), display_text_(display_text), interactive_(true) {}

SkColor DialogNotification::GetBackgroundColor() const {
  switch (type_) {
    case DialogNotification::EXPLANATORY_MESSAGE:
    case DialogNotification::WALLET_SIGNIN_PROMO:
    case DialogNotification::WALLET_USAGE_CONFIRMATION:
      return SkColorSetRGB(0x47, 0x89, 0xfa);
    case DialogNotification::REQUIRED_ACTION:
    case DialogNotification::WALLET_ERROR:
    case DialogNotification::AUTOCHECKOUT_ERROR:
      return SkColorSetRGB(0xfc, 0xf3, 0xbf);
    case DialogNotification::SECURITY_WARNING:
    case DialogNotification::VALIDATION_ERROR:
      return SkColorSetRGB(0xde, 0x49, 0x32);
    case DialogNotification::NONE:
      return SK_ColorTRANSPARENT;
  }

  NOTREACHED();
  return SK_ColorTRANSPARENT;
}

SkColor DialogNotification::GetTextColor() const {
  switch (type_) {
    case DialogNotification::REQUIRED_ACTION:
    case DialogNotification::WALLET_ERROR:
    case DialogNotification::AUTOCHECKOUT_ERROR:
      return SK_ColorBLACK;
    case DialogNotification::EXPLANATORY_MESSAGE:
    case DialogNotification::WALLET_SIGNIN_PROMO:
    case DialogNotification::WALLET_USAGE_CONFIRMATION:
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
         type_ == DialogNotification::WALLET_SIGNIN_PROMO ||
         type_ == DialogNotification::WALLET_USAGE_CONFIRMATION;
}

bool DialogNotification::HasCheckbox() const {
  return type_ == DialogNotification::WALLET_USAGE_CONFIRMATION;
}

SuggestionState::SuggestionState(const string16& text,
                                 const gfx::Image& icon,
                                 const string16& extra_text,
                                 const gfx::Image& extra_icon,
                                 bool editable)
    : text(text),
      icon(icon),
      extra_text(extra_text),
      extra_icon(extra_icon),
      editable(editable) {}
SuggestionState::~SuggestionState() {}

}  // namespace autofill

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_types.h"

#include "base/logging.h"

namespace autofill {

DialogNotification::DialogNotification() : type_(NONE) {}

DialogNotification::DialogNotification(Type type, const string16& display_text)
    : type_(type), display_text_(display_text) {}

SkColor DialogNotification::GetBackgroundColor() const {
  switch (type_) {
    case DialogNotification::SUBMISSION_OPTION:
      return SK_ColorBLUE;
    case DialogNotification::REQUIRED_ACTION:
    case DialogNotification::SECURITY_WARNING:
    case DialogNotification::VALIDATION_ERROR:
    case DialogNotification::WALLET_ERROR:
      return SK_ColorRED;
    case DialogNotification::NONE:
      return SK_ColorTRANSPARENT;
  }

  NOTREACHED();
  return SK_ColorTRANSPARENT;
}

}  // namespace autofill

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_VIEWS_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_VIEWS_UTIL_H_

#include <memory>
#include <string>

#include "base/strings/string16.h"

namespace autofill {
class AutofillProfile;
}

namespace views {
class VectorIconButtonDelegate;
class View;
}

namespace payments {

enum class PaymentRequestCommonTags {
  BACK_BUTTON_TAG = 0,
  CLOSE_BUTTON_TAG,
  // This is the max value of tags for controls common to multiple
  // PaymentRequest contexts. Individual screens that handle both common and
  // specific events with tags can start their specific tags at this value.
  PAYMENT_REQUEST_COMMON_TAG_MAX
};

// Creates and returns a header for all the sheets in the PaymentRequest dialog.
// The header contains an optional back arrow button (if |show_back_arrow| is
// true), a |title| label, and a right-aligned X close button. |delegate|
// becomes the delegate for the back and close buttons.
// +---------------------------+
// | <- | Title            | X |
// +---------------------------+
std::unique_ptr<views::View> CreateSheetHeaderView(
    bool show_back_arrow,
    const base::string16& title,
    views::VectorIconButtonDelegate* delegate);

// Creates a view to be displayed in the PaymentRequestDialog.
// |header_view| is the view displayed on top of the dialog, containing title,
// (optional) back button, and close buttons.
// |content_view| is displayed between |header_view| and the pay/cancel buttons.
// The returned view takes ownership of |header_view| and |content_view|.
// +---------------------------+
// |        HEADER VIEW        |
// +---------------------------+
// |          CONTENT          |
// |           VIEW            |
// +---------------------------+
// |            | CANCEL | PAY |
// +---------------------------+
std::unique_ptr<views::View> CreatePaymentView(
    std::unique_ptr<views::View> header_view,
    std::unique_ptr<views::View> content_view);

// Represents formatting options for each of the different contexts in which an
// Address label may be displayed.
enum class AddressStyleType { SUMMARY, DETAILED };

// Extracts and formats descriptive text from the given |profile| to represent
// the address in the context specified by |type|.
std::unique_ptr<views::View> GetShippingAddressLabel(
    AddressStyleType type,
    const std::string& locale,
    const autofill::AutofillProfile& profile);

// Extracts and formats descriptive text from the given |profile| to represent
// the contact info in the context specified by |type|. Includes/excludes name,
// email, and phone fields according to the respective boolean fields.
std::unique_ptr<views::View> GetContactInfoLabel(
    AddressStyleType type,
    const std::string& locale,
    const autofill::AutofillProfile& profile,
    bool show_payer_name,
    bool show_payer_email,
    bool show_payer_phone);

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_VIEWS_UTIL_H_

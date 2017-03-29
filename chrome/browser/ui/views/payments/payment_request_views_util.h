// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_VIEWS_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_VIEWS_UTIL_H_

#include <memory>
#include <string>

#include "base/strings/string16.h"
#include "components/payments/content/payment_request.mojom.h"

namespace autofill {
class AutofillProfile;
}

namespace views {
class Border;
class ButtonListener;
class ImageView;
class Label;
class View;
}

namespace payments {

constexpr int kPaymentRequestRowHorizontalInsets = 16;
constexpr int kPaymentRequestRowVerticalInsets = 8;

// Extra inset relative to the header when a right edge should line up with the
// close button's X rather than its invisible right edge.
constexpr int kPaymentRequestRowExtraRightInset = 8;
constexpr int kPaymentRequestButtonSpacing = 10;

enum class PaymentRequestCommonTags {
  BACK_BUTTON_TAG = 0,
  CLOSE_BUTTON_TAG,
  PAY_BUTTON_TAG,
  // This is the max value of tags for controls common to multiple
  // PaymentRequest contexts. Individual screens that handle both common and
  // specific events with tags can start their specific tags at this value.
  PAYMENT_REQUEST_COMMON_TAG_MAX
};

// Creates and returns a header for all the sheets in the PaymentRequest dialog.
// The header contains an optional back arrow button (if |show_back_arrow| is
// true), a |title| label. |delegate| becomes the delegate for the back and
// close buttons.
// +---------------------------+
// | <- | Title                |
// +---------------------------+
std::unique_ptr<views::View> CreateSheetHeaderView(
    bool show_back_arrow,
    const base::string16& title,
    views::ButtonListener* delegate);

// Returns an instrument image view for the given |icon_resource_id|. Includes
// a rounded rect border. Callers need to set the size of the resulting
// ImageView. Callers should set a |tooltip_text|.
std::unique_ptr<views::ImageView> CreateInstrumentIconView(
    int icon_resource_id,
    const base::string16& tooltip_text);

std::unique_ptr<views::View> CreateProductLogoFooterView();

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
    bool show_payer_phone,
    bool show_payer_email);

// Creates a views::Border object that can paint the gray horizontal ruler used
// as a separator between items in the Payment Request dialog.
std::unique_ptr<views::Border> CreatePaymentRequestRowBorder();

// Creates a label with a bold font.
std::unique_ptr<views::Label> CreateBoldLabel(const base::string16& text);

base::string16 GetShippingAddressSectionString(
    payments::mojom::PaymentShippingType shipping_type);
base::string16 GetShippingOptionSectionString(
    payments::mojom::PaymentShippingType shipping_type);

std::unique_ptr<views::View> CreateShippingOptionLabel(
    payments::mojom::PaymentShippingOption* shipping_option,
    const base::string16& formatted_amount);

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_VIEWS_UTIL_H_

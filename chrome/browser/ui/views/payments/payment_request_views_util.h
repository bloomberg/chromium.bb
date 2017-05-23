// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_VIEWS_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_VIEWS_UTIL_H_

#include <memory>
#include <string>

#include "base/strings/string16.h"
#include "components/payments/mojom/payment_request.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"

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

class PaymentOptionsProvider;
class PaymentsProfileComparator;
enum class PaymentShippingType;

constexpr int kPaymentRequestRowHorizontalInsets = 16;
constexpr int kPaymentRequestRowVerticalInsets = 8;

// Extra inset relative to the header when a right edge should line up with the
// close button's X rather than its invisible right edge.
constexpr int kPaymentRequestRowExtraRightInset = 8;
constexpr int kPaymentRequestButtonSpacing = 10;

// Dimensions of the dialog itself.
constexpr int kDialogMinWidth = 512;
constexpr int kDialogHeight = 450;

// Fixed width of the amount sections in the payment sheet and the order summary
// sheet, in pixels.
constexpr int kAmountSectionWidth = 96;

enum class PaymentRequestCommonTags {
  BACK_BUTTON_TAG = 0,
  CLOSE_BUTTON_TAG,
  PAY_BUTTON_TAG,
  // This is the max value of tags for controls common to multiple
  // PaymentRequest contexts. Individual screens that handle both common and
  // specific events with tags can start their specific tags at this value.
  PAYMENT_REQUEST_COMMON_TAG_MAX
};

int GetActualDialogWidth();

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
// the address in the context specified by |type|. If |error| is specified,
// this will display it as the last item in an error state. |disabled_state|
// will make the various label lines look disabled.
std::unique_ptr<views::View> GetShippingAddressLabelWithError(
    AddressStyleType type,
    const std::string& locale,
    const autofill::AutofillProfile& profile,
    const base::string16& error,
    bool disabled_state);

// Extracts and formats descriptive text from the given |profile| to represent
// the address in the context specified by |type|. The missing information will
// be computed using |comp| and displayed as the last line in an informative
// manner.
std::unique_ptr<views::View> GetShippingAddressLabelWithMissingInfo(
    AddressStyleType type,
    const std::string& locale,
    const autofill::AutofillProfile& profile,
    const PaymentsProfileComparator& comp);

// Extracts and formats descriptive text from the given |profile| to represent
// the contact info in the context specified by |type|. Includes/excludes name,
// email, and phone fields according to the respective boolean fields.
std::unique_ptr<views::View> GetContactInfoLabel(
    AddressStyleType type,
    const std::string& locale,
    const autofill::AutofillProfile& profile,
    const PaymentOptionsProvider& options,
    const PaymentsProfileComparator& comp);

// Creates a views::Border object with |insets| that can paint the gray
// horizontal ruler used as a separator between items in the Payment Request
// dialog.
std::unique_ptr<views::Border> CreatePaymentRequestRowBorder(
    SkColor color,
    const gfx::Insets& insets);

// Creates a label with a bold font.
std::unique_ptr<views::Label> CreateBoldLabel(const base::string16& text);

// Creates a 2 line label containing |shipping_option|'s label and amount. If
// |emphasize_label| is true, the label part will be in medium weight.
std::unique_ptr<views::View> CreateShippingOptionLabel(
    payments::mojom::PaymentShippingOption* shipping_option,
    const base::string16& formatted_amount,
    bool emphasize_label);

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_VIEWS_UTIL_H_

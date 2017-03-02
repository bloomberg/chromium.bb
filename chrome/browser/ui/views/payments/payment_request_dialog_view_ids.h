// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_VIEW_IDS_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_VIEW_IDS_H_

#include "components/autofill/core/browser/field_types.h"

// This defines an enumeration of IDs that can uniquely identify a view within
// the scope of the Payment Request Dialog.

namespace payments {

enum class DialogViewID : int {
  VIEW_ID_NONE = autofill::MAX_VALID_FIELD_TYPE,

  // The following are views::Button (clickable).
  PAYMENT_SHEET_CONTACT_INFO_SECTION,
  PAYMENT_SHEET_PAYMENT_METHOD_SECTION,
  PAYMENT_SHEET_SHIPPING_SECTION,
  PAYMENT_SHEET_SUMMARY_SECTION,
  PAYMENT_METHOD_ADD_CARD_BUTTON,
  EDITOR_SAVE_BUTTON,
  PAY_BUTTON,

  // The following are StyledLabel objects.
  ORDER_SUMMARY_TOTAL_AMOUNT_LABEL,
  ORDER_SUMMARY_LINE_ITEM_1,
  ORDER_SUMMARY_LINE_ITEM_2,
  ORDER_SUMMARY_LINE_ITEM_3,

  // The following are views contained within the Payment Method Sheet.
  PAYMENT_METHOD_SHEET_LIST_VIEW,

  // Used in selectable rows. Each row in a view reuses this ID, but the ID is
  // unique at the scope of the parent row.
  CHECKMARK_VIEW,

  // The following are views contained within the Contact Info Sheet.
  CONTACT_INFO_ITEM_CHECKMARK_VIEW,

  // Used to label the error labels with an offset, which gets added to
  // the Autofill type value they represent (for tests).
  ERROR_LABEL_OFFSET,
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_VIEW_IDS_H_

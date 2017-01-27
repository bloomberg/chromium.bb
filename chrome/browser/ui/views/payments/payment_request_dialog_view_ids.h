// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_VIEW_IDS_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_VIEW_IDS_H_

// This defines an enumeration of IDs that can uniquely identify a view within
// the scope of the Payment Request Dialog.

namespace payments {

enum class DialogViewID : int {
  VIEW_ID_NONE,

  // The following are views::Button (clickable).
  PAYMENT_SHEET_CONTACT_INFO_SECTION,
  PAYMENT_SHEET_PAYMENT_METHOD_SECTION,
  PAYMENT_SHEET_SHIPPING_SECTION,
  PAYMENT_SHEET_SUMMARY_SECTION,

  // The following are StyledLabel objects.
  ORDER_SUMMARY_TOTAL_AMOUNT_LABEL,
  ORDER_SUMMARY_LINE_ITEM_1,
  ORDER_SUMMARY_LINE_ITEM_2,
  ORDER_SUMMARY_LINE_ITEM_3,
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_VIEW_IDS_H_

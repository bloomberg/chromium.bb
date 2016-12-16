// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_VIEWS_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_VIEWS_UTIL_H_

#include <memory>

#include "base/strings/string16.h"

namespace views {
class View;
}

namespace payments {

// Creates a view to be displayed in the PaymentRequestDialog. |title| is the
// text displayed on top of the dialog and |content_view| is displayed between
// the title and the pay/cancel buttons. The returned view takes ownership of
// |content_view|.
std::unique_ptr<views::View> CreatePaymentView(
    const base::string16& title, std::unique_ptr<views::View> content_view);

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_VIEWS_UTIL_H_

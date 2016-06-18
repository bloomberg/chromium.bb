// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.text.TextUtils;

import org.chromium.chrome.browser.payments.ui.PaymentOption;

/**
 * The locally stored contact details.
 */
public class AutofillContact extends PaymentOption {
    private final String mPayerEmail;
    private final String mPayerPhone;

    /**
     * Builds contact details.
     *
     * @param email Email address.
     * @param phone Phone number.
     */
    public AutofillContact(String email, String phone) {
        super(null, TextUtils.isEmpty(phone) ? email : phone,
                TextUtils.isEmpty(phone) ? null : email, PaymentOption.NO_ICON);
        mPayerEmail = TextUtils.isEmpty(email) ? null : email;
        mPayerPhone = TextUtils.isEmpty(phone) ? null : phone;
        assert mPayerEmail != null || mPayerPhone != null;
    }

    /**
     * Returns the email address for mojo.
     *
     * @return Email address. Null if merchant did not request it.
     */
    public String getPayerEmail() {
        return mPayerEmail;
    }

    /**
     * Returns the phone number for mojo.
     *
     * @return Phone number. Null if merchant did not request it.
     */
    public String getPayerPhone() {
        return mPayerPhone;
    }
}

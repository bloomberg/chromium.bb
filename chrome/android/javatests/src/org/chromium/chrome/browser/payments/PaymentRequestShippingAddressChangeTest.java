// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requires shipping address to calculate shipping.
 */
public class PaymentRequestShippingAddressChangeTest extends PaymentRequestTestBase {
    public PaymentRequestShippingAddressChangeTest() {
        // This merchant requests the shipping address first before providing any shipping options.
        // The result printed from this site is the shipping address change, not the Payment
        // Response.
        super("payment_request_shipping_address_change_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has a shipping address on disk.
        String billingAddressId = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "California", "Los Angeles", "", "90291",
                "", "US", "650-253-0000", "", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.pr_visa,
                billingAddressId, "" /* serverId */));
    }

    /**
     * Tests the format of the shipping address that is sent to the merchant when the user changes
     * the shipping address selection.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testShippingAddressChangeFormat()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Select a shipping address and cancel out.
        triggerUIAndWait(getReadyForInput());
        clickInShippingSummaryAndWait(R.id.payments_section, getReadyForInput());
        clickOnShippingAddressSuggestionOptionAndWait(0, getReadyForInput());
        clickAndWait(R.id.close_button, getDismissed());

        // The phone number should be formatted to the internation format.
        expectResultContains(new String[] {"Jon Doe", "Google", "340 Main St", "CA", "Los Angeles",
                "90291", "+16502530000", "US"});
    }
}

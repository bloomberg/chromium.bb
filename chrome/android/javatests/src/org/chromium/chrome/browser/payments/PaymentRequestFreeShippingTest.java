// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test with a single card and a single shipping address.
 */
public class PaymentRequestFreeShippingTest extends PaymentRequestTestBase {
    public PaymentRequestFreeShippingTest() {
        super("payment_request_free_shipping_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        helper.setProfile(new AutofillProfile("", "https://www.example.com", true, "Jon Doe",
                "Acme", "123 Main St", "CA", "Los Angeles", "", "12345", "", "US", "", "",
                "en-US"));
        helper.setCreditCard(new CreditCard("", "https://www.example.com", true, true, "Jon Smith",
                "4111111111111111", "Visa ***1111", "12", "2050", "visa", 0));
    }

    @MediumTest
    public void testPay() throws InterruptedException, ExecutionException, TimeoutException {
        triggerPaymentUI();
        clickPrimaryPaymentUIButton();
        typeInCvc("123");
        clickPrimaryCVCPromptButton();
        waitForPaymentUIHidden();
        expectResultContains(new String[] {"visa", "4111111111111111", "12", "2050", "123",
                "Jon Smith", "123 Main St", "Acme", "CA", "Los Angeles", "12345", "US", "en",
                "freeShippingOption"});
    }
}

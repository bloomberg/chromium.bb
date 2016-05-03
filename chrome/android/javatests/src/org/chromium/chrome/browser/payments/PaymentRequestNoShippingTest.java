// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test with a single card and no shipping address.
 */
public class PaymentRequestNoShippingTest extends PaymentRequestTestBase {
    public PaymentRequestNoShippingTest() {
        super("payment_request_no_shipping_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        new AutofillTestHelper().setCreditCard(new CreditCard("", "https://www.example.com", true,
                true, "Jon Smith", "4111111111111111", "Visa ***1111", "12", "2050", "visa", 0));
    }

    @MediumTest
    public void testCloseDialog()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerPaymentUI();
        clickClosePaymentUIButton();
        expectResultContains(new String[] {"Request cancelled"});
    }

    @MediumTest
    public void testEditAndCloseDialog()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerPaymentUI();
        clickSecondaryPaymentUIButton();
        clickClosePaymentUIButton();
        expectResultContains(new String[] {"Request cancelled"});
    }

    @MediumTest
    public void testEditAndCancelDialog()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerPaymentUI();
        clickSecondaryPaymentUIButton();
        clickSecondaryPaymentUIButton();
        expectResultContains(new String[] {"Request cancelled"});
    }

    @MediumTest
    public void testPay() throws InterruptedException, ExecutionException, TimeoutException {
        triggerPaymentUI();
        clickPrimaryPaymentUIButton();
        typeInCvc("123");
        clickPrimaryCVCPromptButton();
        waitForPaymentUIHidden();
        expectResultContains(
                new String[] {"visa", "4111111111111111", "12", "2050", "123", "Jon Smith"});
    }
}

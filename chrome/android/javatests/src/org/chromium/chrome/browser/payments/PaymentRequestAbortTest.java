// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that aborts their payment request.
 */
public class PaymentRequestAbortTest extends PaymentRequestTestBase {
    public PaymentRequestAbortTest() {
        // This merchant aborts the payment request when the "abort" button is clicked.
        super("payment_request_abort_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        new AutofillTestHelper().setCreditCard(new CreditCard("", "https://example.com", true, true,
                "Jon Doe", "4111111111111111", "1111", "12", "2050", "visa", R.drawable.pr_visa));
    }

    /** If the user has not clicked "Pay" yet, then merchant's abort will succeed. */
    @MediumTest
    public void testAbortBeforePayClicked() throws InterruptedException, ExecutionException,
            TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickNodeAndWait("abort", mDismissed);
        expectResultContains(new String[] {"Aborted"});
    }

    /** If the user has already clicked the "Pay" button, then merchant won't be able to abort. */
    @MediumTest
    public void testAbortWhileUnmaskingCard() throws InterruptedException, ExecutionException,
            TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        clickNodeAndWait("abort", mUnableToAbort);
        expectResultContains(new String[] {"Cannot abort"});
    }
}

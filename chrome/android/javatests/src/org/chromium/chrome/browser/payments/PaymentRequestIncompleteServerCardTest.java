// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.DialogInterface;
import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A test for paying with an incomplete server card.
 */
public class PaymentRequestIncompleteServerCardTest extends PaymentRequestTestBase {
    private static final int FIRST_BILLING_ADDRESS = 0;

    public PaymentRequestIncompleteServerCardTest() {
        super("payment_request_no_shipping_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "310-310-6000", "jon.doe@gmail.com", "en-US"));
        helper.addServerCreditCard(new CreditCard("", "https://example.com", false /* isLocal */,
                true /* isCached */, "Jon Doe", "4111111111111111", "1111", "12", "2050", "visa",
                R.drawable.pr_visa, "" /* billing address */, "" /* serverId */));
    }

    /** Click [PAY] and dismiss the card unmask dialog. */
    @MediumTest
    @Feature({"Payments"})
    public void testPayAndDontUnmask() throws InterruptedException, ExecutionException,
           TimeoutException {
        triggerUIAndWait(mReadyForInput);
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);
        clickInPaymentMethodAndWait(R.id.payments_first_radio_button, mReadyToEdit);
        setSpinnerSelectionsInCardEditorAndWait(new int[] {FIRST_BILLING_ADDRESS},
                mBillingAddressChangeProcessed);
        clickInCardEditorAndWait(R.id.payments_edit_done_button, mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_NEGATIVE, mReadyToPay);
        clickAndWait(R.id.button_secondary, mDismissed);
        expectResultContains(new String[] {"Request cancelled"});
    }
}

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
 * A payment integration test for removing a billing address that is associated with a credit card.
 */
public class PaymentRequestRemoveBillingAddressTest extends PaymentRequestTestBase {
    public PaymentRequestRemoveBillingAddressTest() {
        super("payment_request_no_shipping_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jane Smith", "Google", "1600 Amphitheatre Pkwy", "CA", "Mountain View", "",
                "94043", "", "US", "555-555-5555", "jane.smith@google.com", "en-US"));
        String billingAddressId = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "555-555-5555", "jon.doe@google.com", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Alice",
                "4111111111111111", "1111", "1", "2050", "visa", R.drawable.pr_visa,
                billingAddressId, "" /* serverId */));
        helper.deleteProfile(billingAddressId);
    }

    /**
     * The billing address for the credit card has been removed. Using this card should bring up an
     * editor that requires selecting a new billing address.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testPayWithCard()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyForInput);

        // Expand the payment section.
        clickInPaymentMethodAndWait(R.id.payments_section, mReadyForInput);

        // Selecting the credit card should bring up the editor.
        clickInPaymentMethodAndWait(R.id.payments_first_radio_button, mReadyToEdit);

        // Tapping "save" in the editor should trigger a validation error.
        clickInCardEditorAndWait(R.id.payments_edit_done_button, mEditorValidationError);

        // Fix the validation error by selecting a billing address.
        setSpinnerSelectionsInCardEditorAndWait(
                new int[] {DECEMBER, NEXT_YEAR, FIRST_BILLING_ADDRESS},
                mBillingAddressChangeProcessed);

        // Tapping "save" in the editor now should close the editor dialog and enable the "pay"
        // button.
        clickInCardEditorAndWait(R.id.payments_edit_done_button, mReadyToPay);

        // Pay with this card.
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);
        expectResultContains(new String[] {"4111111111111111", "Alice", "12", "123", "Jane Smith",
                "Google", "1600 Amphitheatre Pkwy", "CA", "Mountain View", "94043", "US",
                "555-555-5555", "en-US"});
    }
}

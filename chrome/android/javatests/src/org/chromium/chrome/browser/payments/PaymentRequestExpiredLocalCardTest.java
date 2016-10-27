// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.DialogInterface;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a user that pays with an expired local credit card.
 */
public class PaymentRequestExpiredLocalCardTest extends PaymentRequestTestBase {
    public PaymentRequestExpiredLocalCardTest() {
        super("payment_request_free_shipping_test.html");
    }

    AutofillTestHelper mHelper;
    String mCreditCardId;

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        mHelper = new AutofillTestHelper();
        // The user has a shipping address on disk.
        String billingAddressId = mHelper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "555-555-5555", "", "en-US"));
        // Create an expired credit card
        mCreditCardId = mHelper.setCreditCard(new CreditCard("", "https://example.com", true, true,
                "Jon Doe", "4111111111111111", "1111", "1", "2016", "visa", R.drawable.pr_visa,
                billingAddressId, "" /* serverId */));
    }

    /**
     * Tests that the credit card unmask prompt includes inputs for a new expiration date if the
     * credit card is expired. Also tests that the user can pay once they have entered a new valid
     * expiration date and that merchant receives the updated data.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testPayWithExpiredCard_ValidExpiration()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInExpiredCardUnmaskDialogAndWait(
                new int[] {R.id.expiration_month, R.id.expiration_year, R.id.card_unmask_input},
                new String[] {"11", "26", "123"}, mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);
        expectResultContains(new String[] {"Jon Doe", "4111111111111111", "11", "2026", "visa",
                "123", "Google", "340 Main St", "CA", "Los Angeles", "90291", "US", "en",
                "freeShippingOption"});
    }

    /**
     * Tests that the new expiration date entered in the unmasking prompt for an expired card
     * overwrites that card's old expiration date.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testPayWithExpiredCard_NewExpirationSaved()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInExpiredCardUnmaskDialogAndWait(
                new int[] {R.id.expiration_month, R.id.expiration_year, R.id.card_unmask_input},
                new String[] {"11", "26", "123"}, mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);

        // Make sure the new expiration date was saved.
        CreditCard storedCard = mHelper.getCreditCard(mCreditCardId);
        assertEquals("11", storedCard.getMonth());
        assertEquals("2026", storedCard.getYear());
    }
}

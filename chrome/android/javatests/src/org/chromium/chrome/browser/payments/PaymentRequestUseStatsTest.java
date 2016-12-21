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
 * A payment integration test to validate that the use stats of Autofill profiles and credit cards
 * updated when they are used to complete a Payment Request transaction.
 */
public class PaymentRequestUseStatsTest extends PaymentRequestTestBase {
    public PaymentRequestUseStatsTest() {
        // This merchant provides free shipping worldwide.
        super("payment_request_free_shipping_test.html");
    }

    AutofillTestHelper mHelper;
    String mBillingAddressId;
    String mCreditCardId;

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        mHelper = new AutofillTestHelper();
        // The user has a shipping address and a credit card associated with that address on disk.
        mBillingAddressId = mHelper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "555-555-5555", "", "en-US"));
        mCreditCardId = mHelper.setCreditCard(new CreditCard("", "https://example.com", true, true,
                "Jon Doe", "4111111111111111", "1111", "12", "2050", "visa", R.drawable.pr_visa,
                mBillingAddressId, "" /* serverId */));
        // Set specific use stats for the profile and credit card.
        mHelper.setProfileUseStatsForTesting(mBillingAddressId, 20, 5000);
        mHelper.setCreditCardUseStatsForTesting(mCreditCardId, 1, 5000);
    }

    /** Expect that using a profile and credit card to pay updates their usage stats. */
    @MediumTest
    @Feature({"Payments"})
    public void testLogProfileAndCreditCardUse() throws InterruptedException, ExecutionException,
            TimeoutException {
        triggerUIAndWait(mReadyToPay);

        // Get the current date value just before the start of the Payment Request.
        long timeBeforeRecord = mHelper.getCurrentDateForTesting();

        // Proceed with the payment.
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);

        // Get the current date value just after the end of the Payment Request.
        long timeAfterRecord = mHelper.getCurrentDateForTesting();

        // Make sure the use counts were incremented and the use dates were set to the current time.
        assertEquals(21, mHelper.getProfileUseCountForTesting(mBillingAddressId));
        assertTrue(timeBeforeRecord <= mHelper.getProfileUseDateForTesting(mBillingAddressId));
        assertTrue(timeAfterRecord >= mHelper.getProfileUseDateForTesting(mBillingAddressId));
        assertEquals(2, mHelper.getCreditCardUseCountForTesting(mCreditCardId));
        assertTrue(timeBeforeRecord <= mHelper.getCreditCardUseDateForTesting(mCreditCardId));
        assertTrue(timeAfterRecord >= mHelper.getCreditCardUseDateForTesting(mCreditCardId));
    }
}

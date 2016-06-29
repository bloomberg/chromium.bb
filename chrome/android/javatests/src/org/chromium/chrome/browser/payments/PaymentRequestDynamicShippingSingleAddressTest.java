// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.DialogInterface;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requires shipping address to calculate shipping
 * and user that has a single address stored in autofill settings.
 */
public class PaymentRequestDynamicShippingSingleAddressTest extends PaymentRequestTestBase {
    public PaymentRequestDynamicShippingSingleAddressTest() {
        // This merchant requests the shipping address first before providing any shipping options.
        super("payment_request_dynamic_shipping_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has a shipping address on disk.
        String billingAddressId = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "555-555-5555", "", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.pr_visa,
                billingAddressId));
    }

    /** The shipping address should not be selected in UI by default. */
    @MediumTest
    public void testAddressNotSelected()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyForInput);
        assertEquals("Select shipping", getAddressSectionLabel());
    }

    /** Expand the shipping address section, select an address, and click "Pay." */
    @MediumTest
    public void testSelectValidAddressAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyForInput);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);
        clickInShippingAddressAndWait(R.id.payments_first_radio_button, mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);
        expectResultContains(new String[] {"Jon Doe", "4111111111111111", "12", "2050", "visa",
                "123", "Google", "340 Main St", "CA", "Los Angeles", "90291", "US", "en",
                "californiaShippingOption"});
    }

    /** Attempt to add an invalid address and cancel the transaction. */
    @MediumTest
    public void testAddInvalidAddressAndCancel()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyForInput);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);
        clickInShippingAddressAndWait(R.id.payments_add_option_button, mReadyToEdit);
        clickInEditorAndWait(R.id.payments_edit_done_button, mEditorValidationError);
        clickInEditorAndWait(R.id.payments_edit_cancel_button, mReadyToClose);
        clickAndWait(R.id.close_button, mDismissed);
        expectResultContains(new String[] {"Request cancelled"});
    }

    /** Add a valid address and complete the transaction. */
    @MediumTest
    public void testAddAddressAndPay()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyForInput);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);
        clickInShippingAddressAndWait(R.id.payments_add_option_button, mReadyToEdit);
        setTextInEditorAndWait(new String[] {"Bob", "Google", "1600 Amphitheatre Pkwy",
                "Mountain View", "CA", "94043", "999-999-9999"}, mEditorTextUpdate);
        clickInEditorAndWait(R.id.payments_edit_done_button, mReadyToPay);
        clickAndWait(R.id.button_primary, mReadyForUnmaskInput);
        setTextInCardUnmaskDialogAndWait(R.id.card_unmask_input, "123", mReadyToUnmask);
        clickCardUnmaskButtonAndWait(DialogInterface.BUTTON_POSITIVE, mDismissed);
        expectResultContains(new String[] {"Bob", "Google", "1600 Amphitheatre Pkwy",
                "Mountain View", "CA", "94043", "999-999-9999"});
    }
}

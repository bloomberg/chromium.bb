// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.CardType;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.MainActivityStartCallback;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.modaldialog.ModalDialogProperties;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for updateWith().
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestUpdateWithTest implements MainActivityStartCallback {
    @Rule
    public PaymentRequestTestRule mRule =
            new PaymentRequestTestRule("payment_request_update_with_test.html", this);

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        helper.setProfile(new AutofillProfile("" /* guid */, "https://www.example.com" /* origin */,
                "Lisa Simpson", "Acme Inc.", "123 Main", "California", "Los Angeles", "", "90210",
                "", "US", "555 123-4567", "lisa@simpson.com", ""));
        String billingAddressId = helper.setProfile(
                new AutofillProfile("" /* guid */, "https://www.example.com" /* origin */,
                        "Maggie Simpson", "Acme Inc.", "123 Main", "California", "Los Angeles", "",
                        "90210", "", "Uzbekistan", "555 123-4567", "maggie@simpson.com", ""));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.visa_card,
                CardType.UNKNOWN, billingAddressId, "" /* serverId */));
    }

    /**
     * A merchant that calls updateWith() with {} will not cause timeouts in UI.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUpdateWithEmpty() throws Throwable {
        mRule.triggerUIAndWait("updateWithEmpty", mRule.getReadyForInput());
        Assert.assertEquals("USD $5.00", mRule.getOrderSummaryTotal());
        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyForInput());
        mRule.clickOnShippingAddressSuggestionOptionAndWait(1, mRule.getReadyForInput());
        Assert.assertEquals("USD $5.00", mRule.getOrderSummaryTotal());
        mRule.clickAndWait(R.id.button_primary, mRule.getReadyForUnmaskInput());
        mRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mRule.getReadyToUnmask());
        mRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mRule.getDismissed());
        mRule.expectResultContains(new String[] {"freeShipping"});
    }

    /** A merchant that calls updateWith() with total will not cause timeouts in UI. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUpdateWithTotal() throws Throwable {
        mRule.triggerUIAndWait("updateWithTotal", mRule.getReadyForInput());
        Assert.assertEquals("USD $5.00", mRule.getOrderSummaryTotal());
        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyForInput());
        mRule.clickOnShippingAddressSuggestionOptionAndWait(1, mRule.getReadyForInput());
        Assert.assertEquals("USD $10.00", mRule.getOrderSummaryTotal());
        mRule.clickAndWait(R.id.button_primary, mRule.getReadyForUnmaskInput());
        mRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mRule.getReadyToUnmask());
        mRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mRule.getDismissed());
        mRule.expectResultContains(new String[] {"freeShipping"});
    }

    /** A merchant that calls updateWith() with shipping options will not cause timeouts in UI. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUpdateWithShippingOptions() throws Throwable {
        mRule.triggerUIAndWait("updateWithShippingOptions", mRule.getReadyForInput());
        Assert.assertEquals("USD $5.00", mRule.getOrderSummaryTotal());
        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyForInput());
        mRule.clickOnShippingAddressSuggestionOptionAndWait(1, mRule.getReadyForInput());
        Assert.assertEquals("USD $5.00", mRule.getOrderSummaryTotal());
        mRule.clickAndWait(R.id.button_primary, mRule.getReadyForUnmaskInput());
        mRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mRule.getReadyToUnmask());
        mRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mRule.getDismissed());
        mRule.expectResultContains(new String[] {"updatedShipping"});
    }
}

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requires shipping address to calculate shipping
 * and user that has 5 addresses stored in autofill settings.
 */
public class PaymentRequestDynamicShippingMultipleAddressesTest extends PaymentRequestTestBase {
    public PaymentRequestDynamicShippingMultipleAddressesTest() {
        // This merchant requests the shipping address first before providing any shipping options.
        super("payment_request_dynamic_shipping_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // Create an incomplete (no phone) profile with the highest frecency score.
        String guid1 = helper.setProfile(
                new AutofillProfile("" /* guid */, "https://www.example.com" /* origin */,
                        "Bart Simpson", "Acme Inc.", "123 Main", "California", "Los Angeles", "",
                        "90210", "", "US", "", "bart@simpson.com", ""));

        // Create an incomplete (no phone) profile with a the second highest frecency score.
        String guid2 = helper.setProfile(
                new AutofillProfile("" /* guid */, "https://www.example.com" /* origin */,
                        "Homer Simpson", "Acme Inc.", "123 Main", "California", "Los Angeles", "",
                        "90210", "", "US", "", "homer@simpson.com", ""));

        // Create a complete profile with a middle frecency score.
        String guid3 = helper.setProfile(
                new AutofillProfile("" /* guid */, "https://www.example.com" /* origin */,
                        "Lisa Simpson", "Acme Inc.", "123 Main", "California", "Los Angeles", "",
                        "90210", "", "US", "555 123-4567", "lisa@simpson.com", ""));

        // Create a complete profile with the second lowest frecency score.
        String guid4 = helper.setProfile(
                new AutofillProfile("" /* guid */, "https://www.example.com" /* origin */,
                        "Maggie Simpson", "Acme Inc.", "123 Main", "California", "Los Angeles", "",
                        "90210", "", "US", "555 123-4567", "maggie@simpson.com", ""));

        // Create an incomplete profile with the lowest frecency score.
        String guid5 = helper.setProfile(
                new AutofillProfile("" /* guid */, "https://www.example.com" /* origin */,
                        "Marge Simpson", "Acme Inc.", "123 Main", "California", "Los Angeles", "",
                        "90210", "", "US", "", "marge@simpson.com", ""));

        // Create a credit card associated witht the fourth profile.
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.pr_visa,
                guid4));

        // Set the use stats so that profile1 has the highest frecency score, profile2 the second
        // highest, profile 3 the second lowest and profile4 the lowest.
        helper.setProfileUseStatsForTesting(guid1, 20, 5000);
        helper.setProfileUseStatsForTesting(guid2, 15, 5000);
        helper.setProfileUseStatsForTesting(guid3, 10, 5000);
        helper.setProfileUseStatsForTesting(guid4, 5, 5000);
        helper.setProfileUseStatsForTesting(guid5, 1, 1);
    }

    /**
     * Make sure the address suggestions are in the correct order and that only the top 4
     * suggestions are shown. They should be ordered by frecency and complete addresses should be
     * suggested first.
     */
    @MediumTest
    public void testShippingAddressSuggestionOrdering()
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait(mReadyForInput);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);
        assertEquals(4, getNumberOfShippingAddressSuggestions());
        assertTrue(getShippingAddressSuggestionLabel(0).contains("Lisa Simpson"));
        assertTrue(getShippingAddressSuggestionLabel(1).contains("Maggie Simpson"));
        assertTrue(getShippingAddressSuggestionLabel(2).contains("Bart Simpson"));
        assertTrue(getShippingAddressSuggestionLabel(3).contains("Homer Simpson"));
    }
}

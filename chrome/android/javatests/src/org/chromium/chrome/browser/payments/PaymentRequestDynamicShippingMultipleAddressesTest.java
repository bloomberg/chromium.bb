// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;

import java.util.ArrayList;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requires shipping address to calculate shipping
 * and user that has 5 addresses stored in autofill settings.
 */
public class PaymentRequestDynamicShippingMultipleAddressesTest extends PaymentRequestTestBase {
    private static final AutofillProfile[] AUTOFILL_PROFILES = {
        // Incomplete profile (missing phone number)
        new AutofillProfile("" /* guid */, "https://www.example.com" /* origin */,
                "Bart Simpson", "Acme Inc.", "123 Main", "California", "Los Angeles", "",
                "90210", "", "US", "", "bart@simpson.com", ""),

        // Incomplete profile.
        new AutofillProfile("" /* guid */, "https://www.example.com" /* origin */,
                "Homer Simpson", "Acme Inc.", "123 Main", "California", "Los Angeles", "",
                "90210", "", "US", "", "homer@simpson.com", ""),

        // Complete profile.
        new AutofillProfile("" /* guid */, "https://www.example.com" /* origin */,
                "Lisa Simpson", "Acme Inc.", "123 Main", "California", "Los Angeles", "",
                "90210", "", "US", "555 123-4567", "lisa@simpson.com", ""),

        // Complete profile in another country.
        new AutofillProfile("" /* guid */, "https://www.example.com" /* origin */,
                "Maggie Simpson", "Acme Inc.", "123 Main", "California", "Los Angeles", "",
                "90210", "", "Uzbekistan", "555 123-4567", "maggie@simpson.com", ""),

        // Incomplete profile.
        new AutofillProfile("" /* guid */, "https://www.example.com" /* origin */,
                "Marge Simpson", "Acme Inc.", "123 Main", "California", "Los Angeles", "",
                "90210", "", "US", "", "marge@simpson.com", "")
    };

    private AutofillProfile[] mProfilesToAdd;
    private int[] mCountsToSet;
    private int[] mDatesToSet;

    public PaymentRequestDynamicShippingMultipleAddressesTest() {
        // This merchant requests the shipping address first before providing any shipping options.
        super("payment_request_dynamic_shipping_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();

        // Add the profiles.
        ArrayList<String> guids = new ArrayList<>();
        for (int i = 0; i < mProfilesToAdd.length; i++) {
            guids.add(helper.setProfile(mProfilesToAdd[i]));
        }

        // Set up the profile use stats.
        for (int i = 0; i < guids.size(); i++) {
            helper.setProfileUseStatsForTesting(guids.get(i), mCountsToSet[i], mDatesToSet[i]);
        }
    }

    /**
     * Make sure the address suggestions are in the correct order and that only the top 4
     * suggestions are shown. They should be ordered by frecency and complete addresses should be
     * suggested first.
     */
    @MediumTest
    public void testShippingAddressSuggestionOrdering()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Create a bunch of profiles, some complete, some incomplete.  Values are set so that the
        // profiles are ordered by frecency.
        mProfilesToAdd = new AutofillProfile[] {
                AUTOFILL_PROFILES[0], AUTOFILL_PROFILES[1], AUTOFILL_PROFILES[2],
                AUTOFILL_PROFILES[3], AUTOFILL_PROFILES[4]};
        mCountsToSet = new int[] {20, 15, 10, 5, 1};
        mDatesToSet = new int[] {5000, 5000, 5000, 5000, 1};

        triggerUIAndWait(mReadyForInput);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);
        assertEquals(4, getNumberOfShippingAddressSuggestions());
        assertTrue(getShippingAddressSuggestionLabel(0).contains("Lisa Simpson"));
        assertTrue(getShippingAddressSuggestionLabel(1).contains("Maggie Simpson"));
        assertTrue(getShippingAddressSuggestionLabel(2).contains("Bart Simpson"));
        assertTrue(getShippingAddressSuggestionLabel(3).contains("Homer Simpson"));
    }

    /**
     * Select a shipping address that the website refuses to accept, which should force the dialog
     * to show an error.
     */
    @MediumTest
    public void testShippingAddresNotAcceptedByMerchant()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Add a profile that is not accepted by the website.
        mProfilesToAdd = new AutofillProfile[] {AUTOFILL_PROFILES[3]};
        mCountsToSet = new int[] {5};
        mDatesToSet = new int[] {5000};

        // Click on the unacceptable shipping address.
        triggerUIAndWait(mReadyForInput);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);
        assertTrue(getShippingAddressSuggestionLabel(0).contains(
                AUTOFILL_PROFILES[3].getFullName()));
        clickOnShippingAddressSuggestionOptionAndWait(0, mSelectionChecked);

        // The string should indicate that the shipping address isn't valid.
        CharSequence actualString = getShippingAddressOptionRowAtIndex(0).getLabelText();
        CharSequence expectedString = getInstrumentation().getTargetContext().getString(
                R.string.payments_unsupported_shipping_address);
        assertEquals(expectedString, actualString);
    }
}

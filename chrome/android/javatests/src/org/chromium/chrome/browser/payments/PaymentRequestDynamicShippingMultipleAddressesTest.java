// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.Feature;
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

        // Incomplete profile (missing street address).
        new AutofillProfile("" /* guid */, "https://www.example.com" /* origin */,
                "Homer Simpson", "Acme Inc.", "", "California", "Los Angeles", "",
                "90210", "", "US", "555 123-4567", "homer@simpson.com", ""),

        // Complete profile.
        new AutofillProfile("" /* guid */, "https://www.example.com" /* origin */,
                "Lisa Simpson", "Acme Inc.", "123 Main", "California", "Los Angeles", "",
                "90210", "", "US", "555 123-4567", "lisa@simpson.com", ""),

        // Complete profile in another country.
        new AutofillProfile("" /* guid */, "https://www.example.com" /* origin */,
                "Maggie Simpson", "Acme Inc.", "123 Main", "California", "Los Angeles", "",
                "90210", "", "Uzbekistan", "555 123-4567", "maggie@simpson.com", ""),

        // Incomplete profile (invalid address).
        new AutofillProfile("" /* guid */, "https://www.example.com" /* origin */,
                "Marge Simpson", "Acme Inc.", "123 Main", "California", "", "",
                "90210", "", "US", "555 123-4567", "marge@simpson.com", ""),

        // Incomplete profile (missing recipient).
        new AutofillProfile("" /* guid */, "https://www.example.com" /* origin */,
                "", "Acme Inc.", "123 Main", "California", "Los Angeles", "",
                "90210", "", "US", "555 123-4567", "lisa@simpson.com", ""),

        // Incomplete profile (need more information).
        new AutofillProfile("" /* guid */, "https://www.example.com" /* origin */,
                "", "Acme Inc.", "123 Main", "California", "", "",
                "90210", "", "US", "555 123-4567", "lisa@simpson.com", ""),
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
    @Feature({"Payments"})
    public void testShippingAddressSuggestionOrdering()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Create two complete and two incomplete profiles. Values are set so that the profiles are
        // ordered by frecency.
        mProfilesToAdd = new AutofillProfile[] {
                AUTOFILL_PROFILES[0], AUTOFILL_PROFILES[2], AUTOFILL_PROFILES[3],
                AUTOFILL_PROFILES[4]};
        mCountsToSet = new int[] {20, 15, 10, 5, 1};
        mDatesToSet = new int[] {5000, 5000, 5000, 5000, 1};

        triggerUIAndWait(mReadyForInput);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);
        assertEquals(4, getNumberOfShippingAddressSuggestions());
        assertTrue(getShippingAddressSuggestionLabel(0).contains("Lisa Simpson"));
        assertTrue(getShippingAddressSuggestionLabel(1).contains("Maggie Simpson"));
        assertTrue(getShippingAddressSuggestionLabel(2).contains("Bart Simpson"));
        assertTrue(getShippingAddressSuggestionLabel(3).contains("Marge Simpson"));
    }

    /**
     * Make sure that a maximum of four profiles are shown to the user.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testShippingAddressSuggestionLimit()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Create five profiles that can be suggested to the user.
        mProfilesToAdd = new AutofillProfile[] {
                AUTOFILL_PROFILES[0], AUTOFILL_PROFILES[2], AUTOFILL_PROFILES[3],
                AUTOFILL_PROFILES[4], AUTOFILL_PROFILES[5]};
        mCountsToSet = new int[] {20, 15, 10, 5, 2, 1};
        mDatesToSet = new int[] {5000, 5000, 5000, 5000, 2, 1};

        triggerUIAndWait(mReadyForInput);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);
        // Only four profiles should be suggested to the user.
        assertEquals(4, getNumberOfShippingAddressSuggestions());
        assertTrue(getShippingAddressSuggestionLabel(0).contains("Lisa Simpson"));
        assertTrue(getShippingAddressSuggestionLabel(1).contains("Maggie Simpson"));
        assertTrue(getShippingAddressSuggestionLabel(2).contains("Bart Simpson"));
        assertTrue(getShippingAddressSuggestionLabel(3).contains("Marge Simpson"));
    }

    /**
     * Make sure that only profiles with a street address are suggested to the user.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testShippingAddressSuggestion_OnlyIncludeProfilesWithStreetAddress()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Create two complete profiles and two incomplete profiles, one of which has no street
        // address.
        mProfilesToAdd = new AutofillProfile[] {
                AUTOFILL_PROFILES[0], AUTOFILL_PROFILES[1], AUTOFILL_PROFILES[2],
                AUTOFILL_PROFILES[3]};
        mCountsToSet = new int[] {15, 10, 5, 1};
        mDatesToSet = new int[] {5000, 5000, 5000, 1};

        triggerUIAndWait(mReadyForInput);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);
        // Only 3 profiles should be suggested, the two complete ones and the incomplete one that
        // has a street address.
        assertEquals(3, getNumberOfShippingAddressSuggestions());
        assertTrue(getShippingAddressSuggestionLabel(0).contains("Lisa Simpson"));
        assertTrue(getShippingAddressSuggestionLabel(1).contains("Maggie Simpson"));
        assertTrue(getShippingAddressSuggestionLabel(2).contains("Bart Simpson"));
    }

    /**
     * Select a shipping address that the website refuses to accept, which should force the dialog
     * to show an error.
     */
    @MediumTest
    @Feature({"Payments"})
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

    /**
     * Make sure the information required message has been displayed for incomplete profile
     * correctly.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testShippingAddressEditRequiredMessage()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Create four incomplete profiles with different missing information.
        mProfilesToAdd = new AutofillProfile[] {AUTOFILL_PROFILES[0], AUTOFILL_PROFILES[4],
                AUTOFILL_PROFILES[5], AUTOFILL_PROFILES[6]};
        mCountsToSet = new int[] {15, 10, 5, 1};
        mDatesToSet = new int[] {5000, 5000, 5000, 1};

        triggerUIAndWait(mReadyForInput);
        clickInShippingSummaryAndWait(R.id.payments_section, mReadyForInput);

        assertEquals(4, getNumberOfShippingAddressSuggestions());
        assertTrue(getShippingAddressSuggestionLabel(0).contains("Phone number required"));
        assertTrue(getShippingAddressSuggestionLabel(1).contains("Invalid address"));
        assertTrue(getShippingAddressSuggestionLabel(2).contains("Recipient required"));
        assertTrue(getShippingAddressSuggestionLabel(3).contains("More information required"));
    }
}

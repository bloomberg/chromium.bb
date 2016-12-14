// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;

import java.util.ArrayList;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests contact details and a user that has
 * multiple contact detail options.
 */
public class PaymentRequestMultipleContactDetailsTest extends PaymentRequestTestBase {
    private static final AutofillProfile[] AUTOFILL_PROFILES = {
            // Incomplete (no phone) profile.
            new AutofillProfile("" /* guid */, "https://www.example.com" /* origin */,
                    "Bart Simpson", "Acme Inc.", "123 Main", "California", "Los Angeles", "",
                    "90210", "", "US", "", "bart@simpson.com", ""),

            // Incomplete (no email) profile.
            new AutofillProfile("" /* guid */, "https://www.example.com" /* origin */,
                    "Homer Simpson", "Acme Inc.", "123 Main", "California", "Los Angeles", "",
                    "90210", "", "US", "555 123-4567", "", ""),

            // Complete profile.
            new AutofillProfile("" /* guid */, "https://www.example.com" /* origin */,
                    "Lisa Simpson", "Acme Inc.", "123 Main", "California", "Los Angeles", "",
                    "90210", "", "US", "555 123-4567", "lisa@simpson.com", ""),

            // Complete profile.
            new AutofillProfile("" /* guid */, "https://www.example.com" /* origin */,
                    "Maggie Simpson", "Acme Inc.", "123 Main", "California", "Los Angeles", "",
                    "90210", "", "US", "555 123-4567", "maggie@simpson.com", ""),

            // Incomplete (no phone and email) profile.
            new AutofillProfile("" /* guid */, "https://www.example.com" /* origin */,
                    "Marge Simpson", "Acme Inc.", "123 Main", "California", "Los Angeles", "",
                    "90210", "", "US", "", "", ""),

            // Incomplete (no name) profile.
            new AutofillProfile("" /* guid */, "https://www.example.com" /* origin */, "",
                    "Acme Inc.", "123 Main", "California", "Los Angeles", "", "90210", "", "US",
                    "555 123-4567", "marge@simpson.com", ""),

    };

    private AutofillProfile[] mProfilesToAdd;
    private int[] mCountsToSet;
    private int[] mDatesToSet;

    public PaymentRequestMultipleContactDetailsTest() {
        // The merchant requests a name, a phone number and an email address.
        super("payment_request_contact_details_test.html");
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
     * Make sure the contact details suggestions are in the correct order and that only the top 4
     * are shown. They should be ordered by frecency and complete contact details should be
     * suggested first.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testContactDetailsSuggestionOrdering()
            throws InterruptedException, ExecutionException, TimeoutException {
        // Set the use stats so that profile[0] has the highest frecency score, profile[1] the
        // second highest, profile[2] the third lowest, profile[3] the second lowest and profile[4]
        // the lowest.
        mProfilesToAdd = new AutofillProfile[] {AUTOFILL_PROFILES[0], AUTOFILL_PROFILES[1],
                AUTOFILL_PROFILES[2], AUTOFILL_PROFILES[3], AUTOFILL_PROFILES[4]};
        mCountsToSet = new int[] {20, 15, 10, 5, 1};
        mDatesToSet = new int[] {5000, 5000, 5000, 5000, 1};

        triggerUIAndWait(mReadyForInput);
        clickInContactInfoAndWait(R.id.payments_section, mReadyForInput);
        assertEquals(4, getNumberOfContactDetailSuggestions());
        assertEquals("Lisa Simpson\n555 123-4567\nlisa@simpson.com",
                getContactDetailsSuggestionLabel(0));
        assertEquals("Maggie Simpson\n555 123-4567\nmaggie@simpson.com",
                getContactDetailsSuggestionLabel(1));
        assertEquals("Bart Simpson\nbart@simpson.com\nPhone number required",
                getContactDetailsSuggestionLabel(2));
        assertEquals(
                "Homer Simpson\n555 123-4567\nEmail required", getContactDetailsSuggestionLabel(3));
    }

    /**
     * Make sure the information required message has been displayed for incomplete contact details
     * correctly.
     */
    @MediumTest
    @Feature({"Payments"})
    public void testContactDetailsEditRequiredMessage()
            throws InterruptedException, ExecutionException, TimeoutException {
        mProfilesToAdd = new AutofillProfile[] {AUTOFILL_PROFILES[0], AUTOFILL_PROFILES[1],
                AUTOFILL_PROFILES[4], AUTOFILL_PROFILES[5]};
        mCountsToSet = new int[] {15, 10, 5, 1};
        mDatesToSet = new int[] {5000, 5000, 5000, 5000};

        triggerUIAndWait(mReadyForInput);
        clickInContactInfoAndWait(R.id.payments_section, mReadyForInput);
        assertEquals(4, getNumberOfContactDetailSuggestions());
        assertEquals("Bart Simpson\nbart@simpson.com\nPhone number required",
                getContactDetailsSuggestionLabel(0));
        assertEquals(
                "Homer Simpson\n555 123-4567\nEmail required", getContactDetailsSuggestionLabel(1));
        assertEquals(
                "Marge Simpson\nMore information required", getContactDetailsSuggestionLabel(2));
        assertEquals("555 123-4567\nmarge@simpson.com\nName required",
                getContactDetailsSuggestionLabel(3));
    }
}

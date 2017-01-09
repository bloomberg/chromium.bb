// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.payments.ui.ContactDetailsSection;
import org.chromium.chrome.browser.payments.ui.PaymentOption;
import org.chromium.chrome.browser.payments.ui.SectionInformation;
import org.chromium.chrome.test.util.ApplicationData;
import org.chromium.content.browser.test.NativeLibraryTestBase;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for the PaymentRequest ContactDetailsSection class.
 */
public class PaymentRequestContactDetailsSectionUnitTest extends NativeLibraryTestBase {
    private ContactEditor mContactEditor;
    private ContactDetailsSection mContactDetailsSection;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        ApplicationData.clearAppData(getInstrumentation().getTargetContext());
        loadNativeLibraryAndInitBrowserProcess();
    }

    private void createContactDetailsSectionWithProfiles(List<AutofillProfile> autofillProfiles,
            boolean requestPayerName, boolean requestPayerPhone, boolean requestPayerEmail) {
        mContactEditor = new ContactEditor(requestPayerName, requestPayerPhone, requestPayerEmail);
        mContactDetailsSection = new ContactDetailsSection(
                getInstrumentation().getTargetContext(), autofillProfiles, mContactEditor);
    }

    /** Tests the creation of the contact list, with most complete first. */
    @SmallTest
    @Feature({"Payments"})
    public void testContactsListIsCreated_MostCompleteFirst() {
        List<AutofillProfile> profiles = new ArrayList<>();
        // Name, phone and email are all different. First entry is incomplete.
        profiles.add(new AutofillProfile("guid-1", "https://www.example.com", "John Major",
                "Acme Inc.", "123 Main", "California", "Los Angeles", "", "90210", "", "US",
                "" /* no phone number */, "jm@example.com", ""));
        profiles.add(new AutofillProfile("guid-2", "https://www.example.com", "Jane Doe",
                "Edge corp.", "123 Main", "Washington", "Seattle", "", "10110", "", "US",
                "555-212-1212", "jane@example.com", ""));

        createContactDetailsSectionWithProfiles(profiles, true /* requestPayerName */,
                true /* requestPayerPhone */, true /* requestPayerEmail */);

        List<PaymentOption> items = mContactDetailsSection.getItemsForTesting();
        assertEquals(2, items.size());
        assertEquals(0, mContactDetailsSection.getSelectedItemIndex());
        // Most complete item is going to be at the top.
        assertEquals("Jane Doe", items.get(0).getLabel());
        assertEquals("555-212-1212", items.get(0).getSublabel());
        assertEquals("jane@example.com", items.get(0).getTertiaryLabel());
        assertEquals(null, items.get(0).getEditMessage());

        assertEquals("John Major", items.get(1).getLabel());
        assertEquals("jm@example.com", items.get(1).getSublabel());
        assertEquals(null, items.get(1).getTertiaryLabel());
        assertEquals("Phone number required", items.get(1).getEditMessage());
    }

    /** Tests the creation of the contact list, with all profiles being complete. */
    @SmallTest
    @Feature({"Payments"})
    public void testContactsListIsCreated_AllComplete() {
        List<AutofillProfile> profiles = new ArrayList<>();
        // Name, phone and email are all different. All entries complete.
        profiles.add(new AutofillProfile("guid-1", "https://www.example.com", "John Major",
                "Acme Inc.", "123 Main", "California", "Los Angeles", "", "90210", "", "US",
                "514-555-1212", "jm@example.com", ""));
        profiles.add(new AutofillProfile("guid-2", "https://www.example.com", "Jane Doe",
                "Edge corp.", "123 Main", "Washington", "Seattle", "", "10110", "", "US",
                "555-212-1212", "jane@example.com", ""));

        createContactDetailsSectionWithProfiles(profiles, true /* requestPayerName */,
                true /* requestPayerPhone */, true /* requestPayerEmail */);

        List<PaymentOption> items = mContactDetailsSection.getItemsForTesting();
        assertEquals(2, items.size());
        assertEquals(0, mContactDetailsSection.getSelectedItemIndex());
        // Since all are complete, the first profile in the list comes up first in the section.
        assertEquals("John Major", items.get(0).getLabel());
        assertEquals("514-555-1212", items.get(0).getSublabel());
        assertEquals("jm@example.com", items.get(0).getTertiaryLabel());
        assertEquals(null, items.get(0).getEditMessage());

        assertEquals("Jane Doe", items.get(1).getLabel());
        assertEquals("555-212-1212", items.get(1).getSublabel());
        assertEquals("jane@example.com", items.get(1).getTertiaryLabel());
        assertEquals(null, items.get(1).getEditMessage());
    }

    /** Tests the creation of the contact list, not requesting the missing value. */
    @SmallTest
    @Feature({"Payments"})
    public void testContactsListIsCreated_NotRequestingMissingValue() {
        List<AutofillProfile> profiles = new ArrayList<>();
        // Entry is incomplete but it will not matter.
        profiles.add(new AutofillProfile("guid-1", "https://www.example.com", "John Major",
                "Acme Inc.", "123 Main", "California", "Los Angeles", "", "90210", "", "US",
                "" /* no phone number */, "jm@example.com", ""));

        createContactDetailsSectionWithProfiles(profiles, true /* requestPayerName */,
                false /* requestPayerPhone */, true /* requestPayerEmail */);

        List<PaymentOption> items = mContactDetailsSection.getItemsForTesting();
        assertEquals(1, items.size());
        assertEquals(0, mContactDetailsSection.getSelectedItemIndex());
        // Since the phone number was not request, there is no error message.
        assertEquals("John Major", items.get(0).getLabel());
        assertEquals("jm@example.com", items.get(0).getSublabel());
        assertEquals(null, items.get(0).getTertiaryLabel());
        assertEquals(null, items.get(0).getEditMessage());
    }

    /** Tests the update of the contact list with complete data. */
    @SmallTest
    @Feature({"Payments"})
    public void testContactsListIsUpdated_WithCompleteAddress() {
        List<AutofillProfile> profiles = new ArrayList<>();
        // First entry is complete.
        profiles.add(new AutofillProfile("guid-1", "https://www.example.com", "John Major",
                "Acme Inc.", "123 Main", "California", "Los Angeles", "", "90210", "", "US",
                "514-555-1212", "jm@example.com", ""));
        createContactDetailsSectionWithProfiles(profiles, true /* requestPayerName */,
                true /* requestPayerPhone */, true /* requestPayerEmail */);

        List<PaymentOption> items = mContactDetailsSection.getItemsForTesting();
        assertEquals(1, items.size());
        assertEquals(0, mContactDetailsSection.getSelectedItemIndex());
        // Only item shows up as expected.
        assertEquals("John Major", items.get(0).getLabel());
        assertEquals("514-555-1212", items.get(0).getSublabel());
        assertEquals("jm@example.com", items.get(0).getTertiaryLabel());
        assertEquals(null, items.get(0).getEditMessage());

        // We update the contact list with a new, complete address.
        AutofillProfile newProfile = new AutofillProfile("guid-2", "https://www.example.com",
                "Jane Doe", "Edge corp.", "123 Main", "Washington", "Seattle", "", "10110", "",
                "US", "555-212-1212", "jane@example.com", "");
        mContactDetailsSection.addOrUpdateWithAutofillAddress(
                new AutofillAddress(getInstrumentation().getTargetContext(), newProfile));

        // We now expect the new item to be last.
        items = mContactDetailsSection.getItemsForTesting();
        assertEquals(2, items.size());
        assertEquals(0, mContactDetailsSection.getSelectedItemIndex());

        assertEquals("John Major", items.get(0).getLabel());
        assertEquals("514-555-1212", items.get(0).getSublabel());
        assertEquals("jm@example.com", items.get(0).getTertiaryLabel());
        assertEquals(null, items.get(0).getEditMessage());

        assertEquals("Jane Doe", items.get(1).getLabel());
        assertEquals("555-212-1212", items.get(1).getSublabel());
        assertEquals("jane@example.com", items.get(1).getTertiaryLabel());
        assertEquals(null, items.get(1).getEditMessage());
    }

    /** Tests the update of the contact list with incomplete */
    @SmallTest
    @Feature({"Payments"})
    public void testContactsListIsUpdated_WithNewButIncomplete() {
        List<AutofillProfile> profiles = new ArrayList<>();
        // Name, phone and email are all different. All entries complete.
        profiles.add(new AutofillProfile("guid-1", "https://www.example.com", "John Major",
                "Acme Inc.", "123 Main", "California", "Los Angeles", "", "90210", "", "US",
                "514-555-1212", "jm@example.com", ""));
        createContactDetailsSectionWithProfiles(profiles, true /* requestPayerName */,
                true /* requestPayerPhone */, true /* requestPayerEmail */);

        List<PaymentOption> items = mContactDetailsSection.getItemsForTesting();
        assertEquals(1, items.size());
        assertEquals(0, mContactDetailsSection.getSelectedItemIndex());
        // Only item shows up as expected.
        assertEquals("John Major", items.get(0).getLabel());
        assertEquals("514-555-1212", items.get(0).getSublabel());
        assertEquals("jm@example.com", items.get(0).getTertiaryLabel());
        assertEquals(null, items.get(0).getEditMessage());

        // We update the contact list with a new address, which has a missing email.
        AutofillProfile newProfile = new AutofillProfile("guid-2", "https://www.example.com",
                "Jane Doe", "Edge corp.", "123 Main", "Washington", "Seattle", "", "10110", "",
                "US", "555-212-1212", "" /* No email */, "");
        mContactDetailsSection.addOrUpdateWithAutofillAddress(
                new AutofillAddress(getInstrumentation().getTargetContext(), newProfile));

        // We now expect the new item, because it is incomplete, to be last.
        items = mContactDetailsSection.getItemsForTesting();
        assertEquals(2, items.size());
        assertEquals(0, mContactDetailsSection.getSelectedItemIndex());

        assertEquals("John Major", items.get(0).getLabel());
        assertEquals("514-555-1212", items.get(0).getSublabel());
        assertEquals("jm@example.com", items.get(0).getTertiaryLabel());
        assertEquals(null, items.get(0).getEditMessage());

        assertEquals("Jane Doe", items.get(1).getLabel());
        assertEquals("555-212-1212", items.get(1).getSublabel());
        assertEquals(null, items.get(1).getTertiaryLabel());
        assertEquals("Email required", items.get(1).getEditMessage());
    }

    /** Tests the update of initially empty contact list. */
    @SmallTest
    @Feature({"Payments"})
    public void testContactsListIsUpdated_InitiallyEmptyWithCompleteAddress() {
        List<AutofillProfile> profiles = new ArrayList<>();
        createContactDetailsSectionWithProfiles(profiles, true /* requestPayerName */,
                true /* requestPayerPhone */, true /* requestPayerEmail */);

        List<PaymentOption> items = mContactDetailsSection.getItemsForTesting();
        assertEquals(null, items);
        assertEquals(
                SectionInformation.NO_SELECTION, mContactDetailsSection.getSelectedItemIndex());

        // We update the contact list with a new, complete address.
        AutofillProfile newProfile = new AutofillProfile("guid-2", "https://www.example.com",
                "Jane Doe", "Edge corp.", "123 Main", "Washington", "Seattle", "", "10110", "",
                "US", "555-212-1212", "jane@example.com", "");
        mContactDetailsSection.addOrUpdateWithAutofillAddress(
                new AutofillAddress(getInstrumentation().getTargetContext(), newProfile));

        // We now expect the new item to be first. The selection is not changed.
        items = mContactDetailsSection.getItemsForTesting();
        assertEquals(1, items.size());
        assertEquals(
                SectionInformation.NO_SELECTION, mContactDetailsSection.getSelectedItemIndex());

        assertEquals("Jane Doe", items.get(0).getLabel());
        assertEquals("555-212-1212", items.get(0).getSublabel());
        assertEquals("jane@example.com", items.get(0).getTertiaryLabel());
        assertEquals(null, items.get(0).getEditMessage());
    }

    /** Tests the update of an existing contact list item. */
    @SmallTest
    @Feature({"Payments"})
    public void testContactsListIsUpdated_UpdateExistingItem() {
        List<AutofillProfile> profiles = new ArrayList<>();
        // This entry is missing an email, which will get added later on.
        profiles.add(new AutofillProfile("guid-1", "https://www.example.com", "John Major",
                "Acme Inc.", "123 Main", "California", "Los Angeles", "", "90210", "", "US",
                "514-555-1212", "" /* No email */, ""));

        createContactDetailsSectionWithProfiles(profiles, true /* requestPayerName */,
                true /* requestPayerPhone */, true /* requestPayerEmail */);

        List<PaymentOption> items = mContactDetailsSection.getItemsForTesting();
        assertEquals(1, items.size());
        assertEquals(
                SectionInformation.NO_SELECTION, mContactDetailsSection.getSelectedItemIndex());
        // Since all are complete, the first profile in the list comes up first in the section.
        assertEquals("John Major", items.get(0).getLabel());
        assertEquals("514-555-1212", items.get(0).getSublabel());
        assertEquals(null, items.get(0).getTertiaryLabel());
        assertEquals("Email required", items.get(0).getEditMessage());

        // We update the contact list with the same profile GUID, complete this time.
        AutofillProfile newProfile = new AutofillProfile("guid-1", "https://www.example.com",
                "John Major", "Acme Inc.", "456 Main", "California", "Los Angeles", "", "90210", "",
                "US", "514-555-1212", "john@example.com", "");
        mContactDetailsSection.addOrUpdateWithAutofillAddress(
                new AutofillAddress(getInstrumentation().getTargetContext(), newProfile));

        items = mContactDetailsSection.getItemsForTesting();
        assertEquals(1, items.size());
        // The item is now complete, but the selection is not changed.
        assertEquals(
                SectionInformation.NO_SELECTION, mContactDetailsSection.getSelectedItemIndex());
        assertEquals("John Major", items.get(0).getLabel());
        assertEquals("514-555-1212", items.get(0).getSublabel());
        assertEquals("john@example.com", items.get(0).getTertiaryLabel());
        assertEquals(null, items.get(0).getEditMessage());
    }

    /** Tests the update of initially empty contacts list with incomplete data. */
    @SmallTest
    @Feature({"Payments"})
    public void testContactsListIsUpdated_InitiallyEmptyWithIncompleteAddress() {
        List<AutofillProfile> profiles = new ArrayList<>();
        createContactDetailsSectionWithProfiles(profiles, true /* requestPayerName */,
                true /* requestPayerPhone */, true /* requestPayerEmail */);

        List<PaymentOption> items = mContactDetailsSection.getItemsForTesting();
        assertEquals(null, items);
        assertEquals(
                SectionInformation.NO_SELECTION, mContactDetailsSection.getSelectedItemIndex());

        // We update the contact list with a new, incomplete address.
        AutofillProfile newProfile = new AutofillProfile("guid-2", "https://www.example.com",
                "Jane Doe", "Edge corp.", "123 Main", "Washington", "Seattle", "", "10110", "",
                "US", "555-212-1212", "" /* no email */, "");
        mContactDetailsSection.addOrUpdateWithAutofillAddress(
                new AutofillAddress(getInstrumentation().getTargetContext(), newProfile));

        // We now expect the new item to be first, but unselected because incomplete.
        items = mContactDetailsSection.getItemsForTesting();
        assertEquals(1, items.size());
        assertEquals(
                SectionInformation.NO_SELECTION, mContactDetailsSection.getSelectedItemIndex());

        assertEquals("Jane Doe", items.get(0).getLabel());
        assertEquals("555-212-1212", items.get(0).getSublabel());
        assertEquals(null, items.get(0).getTertiaryLabel());
        assertEquals("Email required", items.get(0).getEditMessage());
    }
}

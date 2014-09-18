// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.shell.ChromeShellTestBase;

import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.ExecutionException;

/**
 * Tests for Chrome on Android's usage of the PersonalDataManager API.
 */
public class PersonalDataManagerTest extends ChromeShellTestBase {

    private AutofillTestHelper mHelper;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        clearAppData();
        launchChromeShellWithBlankPage();
        assertTrue(waitForActiveShellToBeDoneLoading());

        mHelper = new AutofillTestHelper();
    }

    @SmallTest
    @Feature({"Autofill"})
    public void testAddAndEditProfiles() throws InterruptedException, ExecutionException {
        AutofillProfile profile = new AutofillProfile(
                "" /* guid */, "https://www.example.com" /* origin */,
                "John Smith", "Acme Inc.",
                "1 Main\nApt A", "CA", "San Francisco", "",
                "94102", "",
                "US", "4158889999", "john@acme.inc", "");
        String profileOneGUID = mHelper.setProfile(profile);
        assertEquals(1, mHelper.getNumberOfProfiles());

        AutofillProfile profile2 = new AutofillProfile(
                "" /* guid */, "http://www.example.com" /* origin */,
                "John Hackock", "Acme Inc.",
                "1 Main\nApt A", "CA", "San Francisco", "",
                "94102", "",
                "US", "4158889999", "john@acme.inc", "");
        String profileTwoGUID = mHelper.setProfile(profile2);
        assertEquals(2, mHelper.getNumberOfProfiles());

        profile.setGUID(profileOneGUID);
        profile.setCountryCode("CA");
        mHelper.setProfile(profile);
        assertEquals("Should still have only two profiles", 2, mHelper.getNumberOfProfiles());

        AutofillProfile storedProfile = mHelper.getProfile(profileOneGUID);
        assertEquals(profileOneGUID, storedProfile.getGUID());
        assertEquals("https://www.example.com", storedProfile.getOrigin());
        assertEquals("CA", storedProfile.getCountryCode());
        assertEquals("San Francisco", storedProfile.getLocality());
        assertNotNull(mHelper.getProfile(profileTwoGUID));
    }

    @SmallTest
    @Feature({"Autofill"})
    public void testUpdateLanguageCodeInProfile() throws InterruptedException, ExecutionException {
        AutofillProfile profile = new AutofillProfile(
                "" /* guid */, "https://www.example.com" /* origin */,
                "John Smith", "Acme Inc.",
                "1 Main\nApt A", "CA", "San Francisco", "",
                "94102", "",
                "US", "4158889999", "john@acme.inc", "fr");
        assertEquals("fr", profile.getLanguageCode());
        String profileOneGUID = mHelper.setProfile(profile);
        assertEquals(1, mHelper.getNumberOfProfiles());

        AutofillProfile storedProfile = mHelper.getProfile(profileOneGUID);
        assertEquals(profileOneGUID, storedProfile.getGUID());
        assertEquals("fr", storedProfile.getLanguageCode());
        assertEquals("US", storedProfile.getCountryCode());

        profile.setGUID(profileOneGUID);
        profile.setLanguageCode("en");
        mHelper.setProfile(profile);

        AutofillProfile storedProfile2 = mHelper.getProfile(profileOneGUID);
        assertEquals(profileOneGUID, storedProfile2.getGUID());
        assertEquals("en", storedProfile2.getLanguageCode());
        assertEquals("US", storedProfile2.getCountryCode());
        assertEquals("San Francisco", storedProfile2.getLocality());
        assertEquals("https://www.example.com", storedProfile2.getOrigin());
    }

    @SmallTest
    @Feature({"Autofill"})
    public void testAddAndDeleteProfile() throws InterruptedException, ExecutionException {
        AutofillProfile profile = new AutofillProfile(
                "" /* guid */, "Chrome settings" /* origin */,
                "John Smith", "Acme Inc.",
                "1 Main\nApt A", "CA", "San Francisco", "",
                "94102", "",
                "US", "4158889999", "john@acme.inc", "");
        String profileOneGUID = mHelper.setProfile(profile);
        assertEquals(1, mHelper.getNumberOfProfiles());

        mHelper.deleteProfile(profileOneGUID);
        assertEquals(0, mHelper.getNumberOfProfiles());
    }

    @SmallTest
    @Feature({"Autofill"})
    public void testAddAndEditCreditCards() throws InterruptedException, ExecutionException {
        CreditCard card = new CreditCard(
                "" /* guid */, "https://www.example.com" /* origin */,
                "Visa", "1234123412341234", "", "5", "2020");
        String cardOneGUID = mHelper.setCreditCard(card);
        assertEquals(1, mHelper.getNumberOfCreditCards());

        CreditCard card2 = new CreditCard(
                "" /* guid */, "http://www.example.com" /* origin */,
                "American Express", "1234123412341234", "", "8", "2020");
        String cardTwoGUID = mHelper.setCreditCard(card2);
        assertEquals(2, mHelper.getNumberOfCreditCards());

        card.setGUID(cardOneGUID);
        card.setMonth("10");
        card.setNumber("5678567856785678");
        mHelper.setCreditCard(card);
        assertEquals("Should still have only two cards", 2, mHelper.getNumberOfCreditCards());

        CreditCard storedCard = mHelper.getCreditCard(cardOneGUID);
        assertEquals(cardOneGUID, storedCard.getGUID());
        assertEquals("https://www.example.com", storedCard.getOrigin());
        assertEquals("Visa", storedCard.getName());
        assertEquals("10", storedCard.getMonth());
        assertEquals("5678567856785678", storedCard.getNumber());
        assertEquals("************5678", storedCard.getObfuscatedNumber());
        assertNotNull(mHelper.getCreditCard(cardTwoGUID));
    }

    @SmallTest
    @Feature({"Autofill"})
    public void testAddAndDeleteCreditCard() throws InterruptedException, ExecutionException {
        CreditCard card = new CreditCard(
                "" /* guid */, "Chrome settings" /* origin */,
                "Visa", "1234123412341234", "", "5", "2020");
        String cardOneGUID = mHelper.setCreditCard(card);
        assertEquals(1, mHelper.getNumberOfCreditCards());

        mHelper.deleteCreditCard(cardOneGUID);
        assertEquals(0, mHelper.getNumberOfCreditCards());
    }

    @SmallTest
    @Feature({"Autofill"})
    public void testRespectCountryCodes() throws InterruptedException, ExecutionException {
        // The constructor should accept country names and ISO 3166-1-alpha-2 country codes.
        // getCountryCode() should return a country code.
        AutofillProfile profile1 = new AutofillProfile(
                "" /* guid */, "https://www.example.com" /* origin */,
                "John Smith", "Acme Inc.",
                "1 Main\nApt A", "Quebec", "Montreal", "",
                "H3B 2Y5", "",
                "Canada", "514-670-1234", "john@acme.inc", "");
        String profileGuid1 = mHelper.setProfile(profile1);

        AutofillProfile profile2 = new AutofillProfile(
                "" /* guid */, "https://www.example.com" /* origin */,
                "Greg Smith", "Ucme Inc.",
                "123 Bush\nApt 125", "Quebec", "Montreal", "",
                "H3B 2Y5", "",
                "CA", "514-670-4321", "greg@ucme.inc", "");
        String profileGuid2 = mHelper.setProfile(profile2);

        assertEquals(2, mHelper.getNumberOfProfiles());

        AutofillProfile storedProfile1 = mHelper.getProfile(profileGuid1);
        assertEquals("CA", storedProfile1.getCountryCode());

        AutofillProfile storedProfile2 = mHelper.getProfile(profileGuid2);
        assertEquals("CA", storedProfile2.getCountryCode());
    }

    @SmallTest
    @Feature({"Autofill"})
    public void testMultilineStreetAddress() throws InterruptedException, ExecutionException {
        final String streetAddress1 = "Chez Mireille COPEAU Appartment. 2\n"
                + "Entree A Batiment Jonquille\n"
                + "25 RUE DE L'EGLISE";
        final String streetAddress2 = streetAddress1 + "\n"
                + "Fourth floor\n"
                + "The red bell";
        AutofillProfile profile = new AutofillProfile(
                "" /* guid */, "https://www.example.com" /* origin */,
                "Monsieur Jean DELHOURME", "Acme Inc.",
                streetAddress1,
                "Tahiti", "Mahina", "Orofara",
                "98709", "CEDEX 98703",
                "French Polynesia", "50.71.53", "john@acme.inc", "");
        String profileGuid1 = mHelper.setProfile(profile);
        assertEquals(1, mHelper.getNumberOfProfiles());
        AutofillProfile storedProfile1 = mHelper.getProfile(profileGuid1);
        assertEquals("PF", storedProfile1.getCountryCode());
        assertEquals("Monsieur Jean DELHOURME", storedProfile1.getFullName());
        assertEquals(streetAddress1, storedProfile1.getStreetAddress());
        assertEquals("Tahiti", storedProfile1.getRegion());
        assertEquals("Mahina", storedProfile1.getLocality());
        assertEquals("Orofara", storedProfile1.getDependentLocality());
        assertEquals("98709", storedProfile1.getPostalCode());
        assertEquals("CEDEX 98703", storedProfile1.getSortingCode());
        assertEquals("50.71.53", storedProfile1.getPhoneNumber());
        assertEquals("john@acme.inc", storedProfile1.getEmailAddress());

        profile.setStreetAddress(streetAddress2);
        String profileGuid2 = mHelper.setProfile(profile);
        assertEquals(2, mHelper.getNumberOfProfiles());
        AutofillProfile storedProfile2 = mHelper.getProfile(profileGuid2);
        assertEquals(streetAddress2, storedProfile2.getStreetAddress());
    }

    @SmallTest
    @Feature({"Autofill"})
    public void testLabels()  throws InterruptedException, ExecutionException {
        AutofillProfile profile1 = new AutofillProfile(
                 "" /* guid */, "https://www.example.com" /* origin */,
                 "John Major", "Acme Inc.",
                 "123 Main", "California", "Los Angeles", "",
                 "90210", "",
                 "US", "555 123-4567", "jm@example.com", "");
        // An almost identical profile.
        AutofillProfile profile2 = new AutofillProfile(
                 "" /* guid */, "https://www.example.com" /* origin */,
                 "John Major", "Acme Inc.",
                 "123 Main", "California", "Los Angeles", "",
                 "90210", "",
                 "US", "555 123-4567", "jm-work@example.com", "");
        // A different profile.
        AutofillProfile profile3 = new AutofillProfile(
                 "" /* guid */, "https://www.example.com" /* origin */,
                 "Jasper Lundgren", "",
                 "1500 Second Ave", "California", "Hollywood", "",
                 "90068", "",
                 "US", "555 123-9876", "jasperl@example.com", "");
        // A profile where a lot of stuff is missing.
        AutofillProfile profile4 = new AutofillProfile(
                 "" /* guid */, "https://www.example.com" /* origin */,
                 "Joe Sergeant", "",
                 "", "Texas", "Fort Worth", "",
                 "", "",
                 "US", "", "", "");

        mHelper.setProfile(profile1);
        mHelper.setProfile(profile2);
        mHelper.setProfile(profile3);
        mHelper.setProfile(profile4);

        List<String> expectedLabels = new LinkedList<String>();
        expectedLabels.add("123 Main, Los Angeles, jm@example.com");
        expectedLabels.add("123 Main, Los Angeles, jm-work@example.com");
        expectedLabels.add("1500 Second Ave, Hollywood");
        expectedLabels.add("Fort Worth, Texas");

        List<AutofillProfile> profiles = mHelper.getProfiles();
        assertEquals(expectedLabels.size(), profiles.size());
        for (int i = 0; i < profiles.size(); ++i) {
            int idx = expectedLabels.indexOf(profiles.get(i).getLabel());
            assertFalse(-1 == idx);
            expectedLabels.remove(idx);
        }
    }
}

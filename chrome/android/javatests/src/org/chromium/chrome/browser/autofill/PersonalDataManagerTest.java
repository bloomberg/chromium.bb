// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManager.PersonalDataManagerObserver;
import org.chromium.chrome.testshell.ChromiumTestShellTestBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Tests for Chrome on Android's usage of the PersonalDataManager API.
 */
public class PersonalDataManagerTest extends ChromiumTestShellTestBase {

    private AtomicBoolean mObserverNotified;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        clearAppData();
        launchChromiumTestShellWithBlankPage();
        assertTrue(waitForActiveShellToBeDoneLoading());
        mObserverNotified = registerDataObserver();
    }

    @SmallTest
    @Feature({"Autofill"})
    public void testAddAndEditProfiles() throws InterruptedException {
        AutofillProfile profile = new AutofillProfile("", // guid
                "John Smith", "Acme Inc.", "1 Main", "Apt A", "San Francisco", "CA",
                "94102", "US", "4158889999", "john@acme.inc");
        String profileOneGUID = setProfile(profile);
        assertEquals(1, getNumberOfProfiles());

        AutofillProfile profile2 = new AutofillProfile("", // guid
                "John Hackock", "Acme Inc.", "1 Main", "Apt A", "San Francisco", "CA",
                "94102", "US", "4158889999", "john@acme.inc");
        String profileTwoGUID = setProfile(profile2);
        assertEquals(2, getNumberOfProfiles());

        profile.setGUID(profileOneGUID);
        profile.setCountry("Canada");
        setProfile(profile);
        assertEquals("Should still have only two profile", 2, getNumberOfProfiles());

        AutofillProfile storedProfile = getProfile(profileOneGUID);
        assertEquals(profileOneGUID, storedProfile.getGUID());
        assertEquals("Canada", storedProfile.getCountry());
        assertEquals("San Francisco", storedProfile.getCity());
        assertNotNull(getProfile(profileTwoGUID));
    }

    @SmallTest
    @Feature({"Autofill"})
    public void testAddAndDeleteProfile() throws InterruptedException {
        AutofillProfile profile = new AutofillProfile("", // guid
                "John Smith", "Acme Inc.", "1 Main", "Apt A", "San Francisco", "CA",
                "94102", "US", "4158889999", "john@acme.inc");
        String profileOneGUID = setProfile(profile);
        assertEquals(1, getNumberOfProfiles());

        deleteProfile(profileOneGUID);
        assertEquals(0, getNumberOfProfiles());
    }

    @SmallTest
    @Feature({"Autofill"})
    public void testAddAndEditCreditCards() throws InterruptedException {
        CreditCard card = new CreditCard("", // guid,
                "Visa", "1234123412341234", "", "5", "2020");
        String cardOneGUID = setCreditCard(card);
        assertEquals(1, getNumberOfCreditCards());

        CreditCard card2 = new CreditCard("", // guid,
                "American Express", "1234123412341234", "", "8", "2020");
        String cardTwoGUID = setCreditCard(card2);
        assertEquals(2, getNumberOfCreditCards());

        card.setGUID(cardOneGUID);
        card.setMonth("10");
        card.setNumber("5678567856785678");
        setCreditCard(card);
        assertEquals("Should still have only two cards", 2, getNumberOfCreditCards());

        CreditCard storedCard = getCreditCard(cardOneGUID);
        assertEquals(cardOneGUID, storedCard.getGUID());
        assertEquals("Visa", storedCard.getName());
        assertEquals("10", storedCard.getMonth());
        assertEquals("5678567856785678", storedCard.getNumber());
        assertEquals("************5678", storedCard.getObfuscatedNumber());
        assertNotNull(getCreditCard(cardTwoGUID));
    }

    @SmallTest
    @Feature({"Autofill"})
    public void testAddAndDeleteCreditCard() throws InterruptedException {
        CreditCard card = new CreditCard("", // guid,
                "Visa", "1234123412341234", "", "5", "2020");
        String cardOneGUID = setCreditCard(card);
        assertEquals(1, getNumberOfCreditCards());

        deleteCreditCard(cardOneGUID);
        assertEquals(0, getNumberOfCreditCards());
    }

    // General helper functions
    // ------------------------
    private AtomicBoolean registerDataObserver() {
        final AtomicBoolean observerNotified = new AtomicBoolean(false);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PersonalDataManager.getInstance().registerDataObserver(
                        new PersonalDataManagerObserver() {
                            @Override
                            public void onPersonalDataChanged() {
                                observerNotified.set(true);
                            }
                        }
                );
            }
        });
        return observerNotified;
    }

    private void waitForDataChanged() throws InterruptedException {
        assertTrue("Observer wasn't notified of PersonalDataManager change.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return mObserverNotified.get();
                    }
        }));
        // Reset member for next iteration.
        mObserverNotified.set(false);
    }

    // Profile helper functions
    // ------------------------
    private AutofillProfile getProfile(final String guid) {
        final AtomicReference<AutofillProfile> profile = new AtomicReference<AutofillProfile>();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                profile.set(PersonalDataManager.getInstance().getProfile(guid));
            }
        });
        return profile.get();
    }

    private int getNumberOfProfiles() {
        final AtomicInteger numProfiles = new AtomicInteger();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                numProfiles.set(PersonalDataManager.getInstance().getProfiles().size());
            }
        });
        return numProfiles.intValue();
    }

    private String setProfile(final AutofillProfile profile) throws InterruptedException {
        final AtomicReference<String> guid = new AtomicReference<String>();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                guid.set(PersonalDataManager.getInstance().setProfile(profile));
            }
        });
        waitForDataChanged();
        return guid.get();
    }

    private void deleteProfile(final String guid) throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
               PersonalDataManager.getInstance().deleteProfile(guid);
            }
        });
        waitForDataChanged();
    }

    // Credit Card helper functions
    // ------------------------
    private CreditCard getCreditCard(final String guid) {
        final AtomicReference<CreditCard> profile = new AtomicReference<CreditCard>();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                profile.set(PersonalDataManager.getInstance().getCreditCard(guid));
            }
        });
        return profile.get();
    }

    private int getNumberOfCreditCards() {
        final AtomicInteger numCreditCards = new AtomicInteger();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                numCreditCards.set(PersonalDataManager.getInstance().getCreditCards().size());
            }
        });
        return numCreditCards.intValue();
    }

    private String setCreditCard(final CreditCard card) throws InterruptedException {
        final AtomicReference<String> guid = new AtomicReference<String>();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                guid.set(PersonalDataManager.getInstance().setCreditCard(card));
            }
        });
        waitForDataChanged();
        return guid.get();
    }

    private void deleteCreditCard(final String guid) throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
               PersonalDataManager.getInstance().deleteCreditCard(guid);
            }
        });
        waitForDataChanged();
    }
}

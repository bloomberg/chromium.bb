// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManager.PersonalDataManagerObserver;

import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Helper class for testing AutofillProfiles.
 */
public class AutofillTestHelper {

    private final Object mObserverNotified;

    public AutofillTestHelper() {
        mObserverNotified = new Object();
        registerDataObserver();
    }

    AutofillProfile getProfile(final String guid) throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<AutofillProfile>() {
            @Override
            public AutofillProfile call() {
                return PersonalDataManager.getInstance().getProfile(guid);
            }
        });
    }

    List<AutofillProfile> getProfilesToSuggest() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<List<AutofillProfile>>() {
            @Override
            public List<AutofillProfile> call() {
                return PersonalDataManager.getInstance().getProfilesToSuggest();
            }
        });
    }

    List<AutofillProfile> getProfilesForSettings() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<List<AutofillProfile>>() {
            @Override
            public List<AutofillProfile> call() {
                return PersonalDataManager.getInstance().getProfilesForSettings();
            }
        });
    }

    int getNumberOfProfilesToSuggest() throws ExecutionException {
        return getProfilesToSuggest().size();
    }

    int getNumberOfProfilesForSettings() throws ExecutionException {
        return getProfilesForSettings().size();
    }

    public String setProfile(final AutofillProfile profile) throws InterruptedException,
            ExecutionException {
        String guid = ThreadUtils.runOnUiThreadBlocking(new Callable<String>() {
            @Override
            public String call() {
                return PersonalDataManager.getInstance().setProfile(profile);
            }
        });
        waitForDataChanged();
        return guid;
    }

    void deleteProfile(final String guid) throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PersonalDataManager.getInstance().deleteProfile(guid);
            }
        });
        waitForDataChanged();
    }

    CreditCard getCreditCard(final String guid) throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<CreditCard>() {
            @Override
            public CreditCard call() {
                return PersonalDataManager.getInstance().getCreditCard(guid);
            }
        });
    }

    List<CreditCard> getCreditCardsToSuggest() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<List<CreditCard>>() {
            @Override
            public List<CreditCard> call() {
                return PersonalDataManager.getInstance().getCreditCardsToSuggest();
            }
        });
    }

    List<CreditCard> getCreditCardsForSettings() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<List<CreditCard>>() {
            @Override
            public List<CreditCard> call() {
                return PersonalDataManager.getInstance().getCreditCardsForSettings();
            }
        });
    }

    int getNumberOfCreditCardsToSuggest() throws ExecutionException {
        return getCreditCardsToSuggest().size();
    }

    int getNumberOfCreditCardsForSettings() throws ExecutionException {
        return getCreditCardsForSettings().size();
    }

    public String setCreditCard(final CreditCard card) throws InterruptedException,
            ExecutionException {
        String guid = ThreadUtils.runOnUiThreadBlocking(new Callable<String>() {
            @Override
            public String call() {
                return PersonalDataManager.getInstance().setCreditCard(card);
            }
        });
        waitForDataChanged();
        return guid;
    }

    public void addServerCreditCard(final CreditCard card)
            throws InterruptedException, ExecutionException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PersonalDataManager.getInstance().addServerCreditCardForTest(card);
            }
        });
        waitForDataChanged();
    }

    void deleteCreditCard(final String guid) throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PersonalDataManager.getInstance().deleteCreditCard(guid);
            }
        });
        waitForDataChanged();
    }

    /**
     * Sets the use |count| and use |date| of the test profile associated with the |guid|.
     * @param guid The GUID of the profile to modify.
     * @param count The use count to assign to the profile. It should be non-negative.
     * @param date The use date to assign to the profile. It represents an absolute point in
     *             coordinated universal time (UTC) represented as microseconds since the Windows
     *             epoch. For more details see the comment header in time.h. It should always be a
     *             positive number.
     */
    void setProfileUseStatsForTesting(final String guid, final int count, final long date)
            throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PersonalDataManager.getInstance().setProfileUseStatsForTesting(guid, count, date);
            }
        });
        waitForDataChanged();
    }

    /**
     * Sets the use |count| and use |date| of the test credit card associated with the |guid|.
     * @param guid The GUID of the credit card to modify.
     * @param count The use count to assign to the credit card. It should be non-negative.
     * @param date The use date to assign to the credit card. It represents an absolute point in
     *             coordinated universal time (UTC) represented as microseconds since the Windows
     *             epoch. For more details see the comment header in time.h. It should always be a
     *             positive number.
     */
    void setCreditCardUseStatsForTesting(final String guid, final int count, final long date)
            throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PersonalDataManager.getInstance().setCreditCardUseStatsForTesting(
                        guid, count, date);
            }
        });
        waitForDataChanged();
    }

    private void registerDataObserver() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PersonalDataManager.getInstance().registerDataObserver(
                        new PersonalDataManagerObserver() {
                            @Override
                            public void onPersonalDataChanged() {
                                synchronized (mObserverNotified) {
                                    mObserverNotified.notifyAll();
                                }
                            }
                        }
                );
            }
        });
    }

    @SuppressWarnings("WaitNotInLoop")
    public void waitForDataChanged() throws InterruptedException {
        synchronized (mObserverNotified) {
            mObserverNotified.wait(3000);
        }
    }
}

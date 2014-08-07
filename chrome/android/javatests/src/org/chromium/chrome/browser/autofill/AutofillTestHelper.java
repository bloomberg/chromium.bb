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

    private Object mObserverNotified;

    public AutofillTestHelper() {
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

    List<AutofillProfile> getProfiles() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<List<AutofillProfile> >() {
            @Override
            public List<AutofillProfile> call() {
                return PersonalDataManager.getInstance().getProfiles();
            }
        });
    }

    int getNumberOfProfiles() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<Integer>() {
            @Override
            public Integer call() {
                return PersonalDataManager.getInstance().getProfiles().size();
            }
        }).intValue();
    }

    String setProfile(final AutofillProfile profile) throws InterruptedException,
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

    int getNumberOfCreditCards() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<Integer>() {
            @Override
            public Integer call() {
                return PersonalDataManager.getInstance().getCreditCards().size();
            }
        }).intValue();
    }

    String setCreditCard(final CreditCard card) throws InterruptedException, ExecutionException {
        String guid = ThreadUtils.runOnUiThreadBlocking(new Callable<String>() {
            @Override
            public String call() {
                return PersonalDataManager.getInstance().setCreditCard(card);
            }
        });
        waitForDataChanged();
        return guid;
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

    private void registerDataObserver() {
        mObserverNotified = new Object();
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

    public void waitForDataChanged() throws InterruptedException {
        synchronized (mObserverNotified) {
            mObserverNotified.wait(3000);
        }
    }
}

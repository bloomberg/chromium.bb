// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.test.util.browser.signin.MockChangeEventChecker;
import org.chromium.sync.signin.AccountManagerHelper;
import org.chromium.sync.signin.ChromeSigninController;
import org.chromium.sync.test.util.AccountHolder;
import org.chromium.sync.test.util.MockAccountManager;

/**
 * Instrumentation tests for {@link SigninHelper}.
 */
public class SigninHelperTest extends InstrumentationTestCase {
    private MockAccountManager mAccountManager;
    private AdvancedMockContext mContext;
    private MockChangeEventChecker mEventChecker;

    @Override
    public void setUp() {
        mContext = new AdvancedMockContext(getInstrumentation().getTargetContext());
        mEventChecker = new MockChangeEventChecker();

        // Mock out the account manager on the device.
        mAccountManager = new MockAccountManager(mContext, getInstrumentation().getContext());
        AccountManagerHelper.overrideAccountManagerHelperForTests(mContext, mAccountManager);
    }

    @SmallTest
    public void testAccountsChangedPref() {
        assertEquals("Should never return true before the pref has ever been set.",
                false, SigninHelper.checkAndClearAccountsChangedPref(mContext));
        assertEquals("Should never return true before the pref has ever been set.",
                false, SigninHelper.checkAndClearAccountsChangedPref(mContext));

        // Mark the pref as set.
        SigninHelper.markAccountsChangedPref(mContext);

        assertEquals("Should return true first time after marking accounts changed",
                true, SigninHelper.checkAndClearAccountsChangedPref(mContext));
        assertEquals("Should only return true first time after marking accounts changed",
                false, SigninHelper.checkAndClearAccountsChangedPref(mContext));
        assertEquals("Should only return true first time after marking accounts changed",
                false, SigninHelper.checkAndClearAccountsChangedPref(mContext));

        // Mark the pref as set again.
        SigninHelper.markAccountsChangedPref(mContext);

        assertEquals("Should return true first time after marking accounts changed",
                true, SigninHelper.checkAndClearAccountsChangedPref(mContext));
        assertEquals("Should only return true first time after marking accounts changed",
                false, SigninHelper.checkAndClearAccountsChangedPref(mContext));
        assertEquals("Should only return true first time after marking accounts changed",
                false, SigninHelper.checkAndClearAccountsChangedPref(mContext));
    }

    @SmallTest
    public void testSimpleAccountRename() {
        setSignedInAccountName("A");
        mEventChecker.insertRenameEvent("A", "B");
        SigninHelper.updateAccountRenameData(mContext, mEventChecker);
        assertEquals("B", getNewSignedInAccountName());
    }

    @DisabledTest // crbug.com/568623
    @SmallTest
    public void testNotSignedInAccountRename() {
        setSignedInAccountName("A");
        mEventChecker.insertRenameEvent("B", "C");
        SigninHelper.updateAccountRenameData(mContext, mEventChecker);
        assertEquals(null, getNewSignedInAccountName());
    }

    @SmallTest
    public void testSimpleAccountRenameTwice() {
        setSignedInAccountName("A");
        mEventChecker.insertRenameEvent("A", "B");
        SigninHelper.updateAccountRenameData(mContext, mEventChecker);
        assertEquals("B", getNewSignedInAccountName());
        mEventChecker.insertRenameEvent("B", "C");
        SigninHelper.updateAccountRenameData(mContext, mEventChecker);
        assertEquals("C", getNewSignedInAccountName());
    }

    @SmallTest
    public void testNotSignedInAccountRename2() {
        setSignedInAccountName("A");
        mEventChecker.insertRenameEvent("B", "C");
        mEventChecker.insertRenameEvent("C", "D");
        SigninHelper.updateAccountRenameData(mContext, mEventChecker);
        assertEquals(null, getNewSignedInAccountName());
    }

    @SmallTest
    public void testChainedAccountRename2() {
        setSignedInAccountName("A");
        mEventChecker.insertRenameEvent("Z", "Y"); // Unrelated.
        mEventChecker.insertRenameEvent("A", "B");
        mEventChecker.insertRenameEvent("Y", "X"); // Unrelated.
        mEventChecker.insertRenameEvent("B", "C");
        mEventChecker.insertRenameEvent("C", "D");
        SigninHelper.updateAccountRenameData(mContext, mEventChecker);
        assertEquals("D", getNewSignedInAccountName());
    }

    @SmallTest
    public void testLoopedAccountRename() {
        setSignedInAccountName("A");
        mEventChecker.insertRenameEvent("Z", "Y"); // Unrelated.
        mEventChecker.insertRenameEvent("A", "B");
        mEventChecker.insertRenameEvent("Y", "X"); // Unrelated.
        mEventChecker.insertRenameEvent("B", "C");
        mEventChecker.insertRenameEvent("C", "D");
        mEventChecker.insertRenameEvent("D", "A"); // Looped.
        Account account = AccountManagerHelper.createAccountFromName("D");
        AccountHolder accountHolder = AccountHolder.create().account(account).build();
        mAccountManager.addAccountHolderExplicitly(accountHolder);
        SigninHelper.updateAccountRenameData(mContext, mEventChecker);
        assertEquals("D", getNewSignedInAccountName());
    }

    private void setSignedInAccountName(String account) {
        ChromeSigninController.get(mContext).setSignedInAccountName(account);
    }

    private String getSignedInAccountName() {
        return ChromeSigninController.get(mContext).getSignedInAccountName();
    }

    private String getNewSignedInAccountName() {
        return SigninHelper.getNewSignedInAccountName(mContext);
    }
}

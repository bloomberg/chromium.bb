// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.signin.MockChangeEventChecker;
import org.chromium.components.signin.AccountManagerHelper;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;

/**
 * Instrumentation tests for {@link SigninHelper}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class SigninHelperTest {
    private FakeAccountManagerDelegate mAccountManager;
    private AdvancedMockContext mContext;
    private MockChangeEventChecker mEventChecker;

    @Before
    public void setUp() {
        mContext = new AdvancedMockContext(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        mEventChecker = new MockChangeEventChecker();

        mAccountManager = new FakeAccountManagerDelegate(mContext);
        AccountManagerHelper.overrideAccountManagerHelperForTests(mContext, mAccountManager);
    }

    @After
    public void tearDown() {
        AccountManagerHelper.resetAccountManagerHelperForTests();
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testAccountsChangedPref() {
        Assert.assertEquals("Should never return true before the pref has ever been set.", false,
                SigninHelper.checkAndClearAccountsChangedPref(mContext));
        Assert.assertEquals("Should never return true before the pref has ever been set.", false,
                SigninHelper.checkAndClearAccountsChangedPref(mContext));

        // Mark the pref as set.
        SigninHelper.markAccountsChangedPref(mContext);

        Assert.assertEquals("Should return true first time after marking accounts changed", true,
                SigninHelper.checkAndClearAccountsChangedPref(mContext));
        Assert.assertEquals("Should only return true first time after marking accounts changed",
                false, SigninHelper.checkAndClearAccountsChangedPref(mContext));
        Assert.assertEquals("Should only return true first time after marking accounts changed",
                false, SigninHelper.checkAndClearAccountsChangedPref(mContext));

        // Mark the pref as set again.
        SigninHelper.markAccountsChangedPref(mContext);

        Assert.assertEquals("Should return true first time after marking accounts changed", true,
                SigninHelper.checkAndClearAccountsChangedPref(mContext));
        Assert.assertEquals("Should only return true first time after marking accounts changed",
                false, SigninHelper.checkAndClearAccountsChangedPref(mContext));
        Assert.assertEquals("Should only return true first time after marking accounts changed",
                false, SigninHelper.checkAndClearAccountsChangedPref(mContext));
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testSimpleAccountRename() {
        setSignedInAccountName("A");
        mEventChecker.insertRenameEvent("A", "B");
        SigninHelper.updateAccountRenameData(mContext, mEventChecker);
        Assert.assertEquals("B", getNewSignedInAccountName());
    }

    @Test
    @DisabledTest(message = "crbug.com/568623")
    @SmallTest
    public void testNotSignedInAccountRename() {
        setSignedInAccountName("A");
        mEventChecker.insertRenameEvent("B", "C");
        SigninHelper.updateAccountRenameData(mContext, mEventChecker);
        Assert.assertEquals(null, getNewSignedInAccountName());
    }

    @Test
    @SmallTest
    public void testSimpleAccountRenameTwice() {
        setSignedInAccountName("A");
        mEventChecker.insertRenameEvent("A", "B");
        SigninHelper.updateAccountRenameData(mContext, mEventChecker);
        Assert.assertEquals("B", getNewSignedInAccountName());
        mEventChecker.insertRenameEvent("B", "C");
        SigninHelper.updateAccountRenameData(mContext, mEventChecker);
        Assert.assertEquals("C", getNewSignedInAccountName());
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testNotSignedInAccountRename2() {
        setSignedInAccountName("A");
        mEventChecker.insertRenameEvent("B", "C");
        mEventChecker.insertRenameEvent("C", "D");
        SigninHelper.updateAccountRenameData(mContext, mEventChecker);
        Assert.assertEquals(null, getNewSignedInAccountName());
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testChainedAccountRename2() {
        setSignedInAccountName("A");
        mEventChecker.insertRenameEvent("Z", "Y"); // Unrelated.
        mEventChecker.insertRenameEvent("A", "B");
        mEventChecker.insertRenameEvent("Y", "X"); // Unrelated.
        mEventChecker.insertRenameEvent("B", "C");
        mEventChecker.insertRenameEvent("C", "D");
        SigninHelper.updateAccountRenameData(mContext, mEventChecker);
        Assert.assertEquals("D", getNewSignedInAccountName());
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testLoopedAccountRename() {
        setSignedInAccountName("A");
        mEventChecker.insertRenameEvent("Z", "Y"); // Unrelated.
        mEventChecker.insertRenameEvent("A", "B");
        mEventChecker.insertRenameEvent("Y", "X"); // Unrelated.
        mEventChecker.insertRenameEvent("B", "C");
        mEventChecker.insertRenameEvent("C", "D");
        mEventChecker.insertRenameEvent("D", "A"); // Looped.
        Account account = AccountManagerHelper.createAccountFromName("D");
        AccountHolder accountHolder = AccountHolder.builder(account).build();
        mAccountManager.addAccountHolderExplicitly(accountHolder);
        SigninHelper.updateAccountRenameData(mContext, mEventChecker);
        Assert.assertEquals("D", getNewSignedInAccountName());
    }

    private void setSignedInAccountName(String account) {
        ChromeSigninController.get().setSignedInAccountName(account);
    }

    private String getSignedInAccountName() {
        return ChromeSigninController.get().getSignedInAccountName();
    }

    private String getNewSignedInAccountName() {
        return SigninHelper.getNewSignedInAccountName(mContext);
    }
}

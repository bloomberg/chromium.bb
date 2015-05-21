// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.sync.signin.ChromeSigninController;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Instrumentation tests for {@link SigninHelper}.
 */
public class SigninHelperTest extends InstrumentationTestCase {
    private AdvancedMockContext mContext;
    private MockChangeEventChecker mEventChecker;

    @Override
    public void setUp() {
        mContext = new AdvancedMockContext(getInstrumentation().getTargetContext());
        mEventChecker = new MockChangeEventChecker();
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

    private void setSignedInAccountName(String account) {
        ChromeSigninController.get(mContext).setSignedInAccountName(account);
    }

    private String getSignedInAccountName() {
        return ChromeSigninController.get(mContext).getSignedInAccountName();
    }

    private String getNewSignedInAccountName() {
        return SigninHelper.getNewSignedInAccountName(mContext);
    }

    private static final class MockChangeEventChecker
            implements SigninHelper.AccountChangeEventChecker {
        private Map<String, List<String>> mEvents =
                new HashMap<String, List<String>>();

        @Override
        public List<String> getAccountChangeEvents(
                Context context, int index, String accountName) {
            List<String> eventsList = getEventList(accountName);
            return eventsList.subList(index, eventsList.size());
        }

        private void insertRenameEvent(String from, String to) {
            List<String> eventsList = getEventList(from);
            eventsList.add(to);
        }

        private List<String> getEventList(String account) {
            List<String> eventsList = mEvents.get(account);
            if (eventsList == null) {
                eventsList = new ArrayList<String>();
                mEvents.put(account, eventsList);
            }
            return eventsList;
        }
    }
}

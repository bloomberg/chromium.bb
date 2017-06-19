// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test;

import android.accounts.Account;
import android.content.Context;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import org.chromium.base.ThreadUtils;
import org.chromium.components.signin.AccountManagerHelper;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;
import org.chromium.components.signin.test.util.SimpleFuture;

/**
 * Test class for {@link AccountManagerHelper}.
 */
public class AccountManagerHelperTest extends InstrumentationTestCase {
    private FakeAccountManagerDelegate mDelegate;
    private AccountManagerHelper mHelper;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        Context context = getInstrumentation().getTargetContext();
        mDelegate = new FakeAccountManagerDelegate(context);
        AccountManagerHelper.overrideAccountManagerHelperForTests(context, mDelegate);
        mHelper = AccountManagerHelper.get();
    }

    @Override
    protected void tearDown() throws Exception {
        AccountManagerHelper.resetAccountManagerHelperForTests();
        super.tearDown();
    }

    @SmallTest
    public void testCanonicalAccount() throws InterruptedException {
        addTestAccount("test@gmail.com", "password");

        assertTrue(hasAccountForName("test@gmail.com"));
        assertTrue(hasAccountForName("Test@gmail.com"));
        assertTrue(hasAccountForName("te.st@gmail.com"));
    }

    // If this test starts flaking, please re-open crbug.com/568636 and make sure there is some sort
    // of stack trace or error message in that bug BEFORE disabling the test.
    @SmallTest
    public void testNonCanonicalAccount() throws InterruptedException {
        addTestAccount("test.me@gmail.com", "password");

        assertTrue(hasAccountForName("test.me@gmail.com"));
        assertTrue(hasAccountForName("testme@gmail.com"));
        assertTrue(hasAccountForName("Testme@gmail.com"));
        assertTrue(hasAccountForName("te.st.me@gmail.com"));
    }

    private Account addTestAccount(String accountName, String password) {
        Account account = AccountManagerHelper.createAccountFromName(accountName);
        AccountHolder.Builder accountHolder =
                AccountHolder.builder(account).password(password).alwaysAccept(true);
        mDelegate.addAccountHolderExplicitly(accountHolder.build());
        return account;
    }

    private boolean hasAccountForName(final String accountName) throws InterruptedException {
        final SimpleFuture<Boolean> result = new SimpleFuture<Boolean>();
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                mHelper.hasAccountForName(accountName, result.createCallback());
            }
        });
        return result.get();
    }
}

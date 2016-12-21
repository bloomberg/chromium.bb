// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test;

import android.accounts.Account;
import android.content.Context;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import org.chromium.components.signin.AccountManagerHelper;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.MockAccountManager;
import org.chromium.components.signin.test.util.SimpleFuture;

/**
 * Test class for {@link AccountManagerHelper}.
 */
public class AccountManagerHelperTest extends InstrumentationTestCase {
    private MockAccountManager mAccountManager;
    private AccountManagerHelper mHelper;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        Context context = getInstrumentation().getTargetContext();
        mAccountManager = new MockAccountManager(context, context);
        AccountManagerHelper.overrideAccountManagerHelperForTests(context, mAccountManager);
        mHelper = AccountManagerHelper.get(context);
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
                AccountHolder.create().account(account).password(password).alwaysAccept(true);
        mAccountManager.addAccountHolderExplicitly(accountHolder.build());
        return account;
    }

    private boolean hasAccountForName(String accountName) throws InterruptedException {
        SimpleFuture<Boolean> result = new SimpleFuture<Boolean>();
        mHelper.hasAccountForName(accountName, result.createCallback());
        return result.get();
    }
}

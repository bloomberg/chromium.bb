// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test;

import android.accounts.Account;
import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;
import org.chromium.components.signin.test.util.SimpleFuture;

/**
 * Test class for {@link AccountManagerFacade}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class AccountManagerFacadeTest {
    private FakeAccountManagerDelegate mDelegate;
    private AccountManagerFacade mHelper;

    @Before
    public void setUp() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        mDelegate = new FakeAccountManagerDelegate(context);
        AccountManagerFacade.overrideAccountManagerFacadeForTests(context, mDelegate);
        mHelper = AccountManagerFacade.get();
    }

    @After
    public void tearDown() throws Exception {
        AccountManagerFacade.resetAccountManagerFacadeForTests();
    }

    @Test
    @SmallTest
    public void testCanonicalAccount() throws InterruptedException {
        addTestAccount("test@gmail.com", "password");

        Assert.assertTrue(hasAccountForName("test@gmail.com"));
        Assert.assertTrue(hasAccountForName("Test@gmail.com"));
        Assert.assertTrue(hasAccountForName("te.st@gmail.com"));
    }

    // If this test starts flaking, please re-open crbug.com/568636 and make sure there is some sort
    // of stack trace or error message in that bug BEFORE disabling the test.
    @Test
    @SmallTest
    public void testNonCanonicalAccount() throws InterruptedException {
        addTestAccount("test.me@gmail.com", "password");

        Assert.assertTrue(hasAccountForName("test.me@gmail.com"));
        Assert.assertTrue(hasAccountForName("testme@gmail.com"));
        Assert.assertTrue(hasAccountForName("Testme@gmail.com"));
        Assert.assertTrue(hasAccountForName("te.st.me@gmail.com"));
    }

    private Account addTestAccount(String accountName, String password) {
        Account account = AccountManagerFacade.createAccountFromName(accountName);
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

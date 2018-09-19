// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test;

import static org.mockito.Mockito.doAnswer;

import android.accounts.Account;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.components.signin.AccountManagerDelegate;
import org.chromium.components.signin.AccountManagerDelegateException;
import org.chromium.components.signin.AccountManagerFacade;

import java.util.concurrent.CountDownLatch;

/**
 * Tests for {@link AccountManagerFacade}. See also {@link AccountManagerFacadeRobolectricTest}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class AccountManagerFacadeTest {
    @Test
    @SmallTest
    public void testIsCachePopulated() throws AccountManagerDelegateException {
        AccountManagerDelegate delegate = Mockito.mock(AccountManagerDelegate.class);

        final Account account = AccountManagerFacade.createAccountFromName("test@gmail.com");
        final CountDownLatch blockGetAccounts = new CountDownLatch(1);
        doAnswer(invocation -> {
            // Block background thread that's trying to get accounts from the delegate.
            blockGetAccounts.await();
            return new Account[] {account};
        }).when(delegate).getAccountsSync();

        AccountManagerFacade.overrideAccountManagerFacadeForTests(delegate);
        AccountManagerFacade facade = AccountManagerFacade.get();

        // Cache shouldn't be populated until getAccountsSync is unblocked.
        Assert.assertFalse(facade.isCachePopulated());

        blockGetAccounts.countDown();
        // Wait for cache population to finish.
        AccountManagerFacade.get().getGoogleAccounts();
        Assert.assertTrue(facade.isCachePopulated());
    }
}

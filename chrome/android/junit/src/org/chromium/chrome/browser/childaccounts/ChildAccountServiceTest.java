// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.childaccounts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.accounts.Account;
import android.content.Context;
import android.content.pm.PackageManager;

import com.google.android.collect.Sets;

import org.chromium.base.CommandLine;
import org.chromium.chrome.browser.childaccounts.ChildAccountService.HasChildAccountCallback;
import org.chromium.sync.signin.AccountManagerHelper;
import org.chromium.sync.test.util.AccountHolder;
import org.chromium.sync.test.util.MockAccountManager;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Matchers;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import java.util.Collections;
import java.util.HashSet;

/**
 * Robolectric-based unit test for ChildAccountService.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChildAccountServiceTest {
    private static final Account USER_ACCOUNT =
            new Account("user@gmail.com", AccountManagerHelper.GOOGLE_ACCOUNT_TYPE);

    private static final Account CHILD_ACCOUNT =
            new Account("child@gmail.com", AccountManagerHelper.GOOGLE_ACCOUNT_TYPE);

    private ChildAccountService mService;

    private MockAccountManager mAccountManager;

    private Context mContext = spy(Robolectric.application);

    private class TestingChildAccountService extends ChildAccountService {
        private Boolean mLastStatus;

        public TestingChildAccountService(Context context) {
            super(context);
        }

        @Override
        protected void onChildAccountStatusUpdated(Boolean oldValue) {
            assertEquals(mLastStatus, oldValue);
            mLastStatus = hasChildAccount();
        }
    }

    private class MockChildAccountCallback implements HasChildAccountCallback {
        @Override
        public void onChildAccountChecked(boolean hasChildAccount) {
            assertEquals(hasChildAccount, mService.hasChildAccount());
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mService = spy(new TestingChildAccountService(mContext));
        when(mContext.getApplicationContext()).thenReturn(mContext);
        when(mContext.checkPermission(
                Matchers.anyString(), Matchers.anyInt(), Matchers.anyInt()))
                .thenReturn(PackageManager.PERMISSION_GRANTED);
        doReturn(true).when(mService).nativeIsChildAccountDetectionEnabled();
        doThrow(new AssertionError()).when(mService).nativeGetIsChildAccount();
        mAccountManager = new MockAccountManager(mContext, mContext);
        AccountManagerHelper.overrideAccountManagerHelperForTests(mContext, mAccountManager);

        CommandLine.init(new String[] {});

        // Run the main looper manually.
        Robolectric.pauseMainLooper();
    }

    @Test
    public void testNoAccounts() {
        // Add no accounts.
        HasChildAccountCallback callback = spy(new MockChildAccountCallback());
        mService.checkHasChildAccount(callback);

        // The child account status is immediately updated to false.
        verify(mService).onChildAccountStatusUpdated(null);

        // The callback is only run on the next turn of the event loop, however.
        verify(callback, never()).onChildAccountChecked(Matchers.anyBoolean());

        Robolectric.idleMainLooper(0);

        verify(callback).onChildAccountChecked(false);
    }

    @Test
    public void testMultipleAccounts() {
        // Add multiple accounts.
        mAccountManager.addAccountHolderExplicitly(
                AccountHolder.create().account(CHILD_ACCOUNT).build());
        mAccountManager.addAccountHolderExplicitly(
                AccountHolder.create().account(USER_ACCOUNT).build());

        HasChildAccountCallback callback = spy(new MockChildAccountCallback());
        mService.checkHasChildAccount(callback);

        // The child account status is immediately updated to false.
        verify(mService).onChildAccountStatusUpdated(null);

        // The callback is only run on the next turn of the event loop, however.
        verify(callback, never()).onChildAccountChecked(Matchers.anyBoolean());

        Robolectric.idleMainLooper(0);

        verify(callback).onChildAccountChecked(false);
    }

    @Test
    public void testSingleRegularAccount() {
        // Add one regular account.
        mAccountManager.addAccountHolderExplicitly(
                AccountHolder.create().account(USER_ACCOUNT).featureSet(null).build());

        HasChildAccountCallback callback = spy(new MockChildAccountCallback());
        mService.checkHasChildAccount(callback);
        Robolectric.idleMainLooper(0);

        // The account features have not been fetched yet, so the child account status is
        // indeterminate.
        verify(mService, never()).onChildAccountStatusUpdated(Matchers.any(Boolean.class));
        verify(callback, never()).onChildAccountChecked(Matchers.anyBoolean());

        // When the account features have been fetched, the child account status is set to false and
        // the callback is run.
        mAccountManager.notifyFeaturesFetched(USER_ACCOUNT, Collections.<String>emptySet());
        Robolectric.idleMainLooper(0);

        verify(mService).onChildAccountStatusUpdated(null);
        verify(callback).onChildAccountChecked(false);
    }

    @Test
    public void testSingleChildAccount() {
        // Add one child account.
        mAccountManager.addAccountHolderExplicitly(
                AccountHolder.create().account(CHILD_ACCOUNT).featureSet(null).build());

        HasChildAccountCallback callback = spy(new MockChildAccountCallback());
        mService.checkHasChildAccount(callback);
        Robolectric.idleMainLooper(0);

        // The account features have not been fetched yet, so the child account status is
        // indeterminate.
        verify(mService, never()).onChildAccountStatusUpdated(Matchers.any(Boolean.class));
        verify(callback, never()).onChildAccountChecked(Matchers.anyBoolean());

        // When the account features have been fetched, the child account status is set to true and
        // the callback is run.
        mAccountManager.notifyFeaturesFetched(
                CHILD_ACCOUNT, Sets.newHashSet(AccountManagerHelper.FEATURE_IS_CHILD_ACCOUNT_KEY));
        Robolectric.idleMainLooper(0);

        verify(mService).onChildAccountStatusUpdated(null);
        verify(callback).onChildAccountChecked(true);
    }

    @Test
    public void testCheckAfterChildAccountStatusHasBeenDetermined() {
        // Add one child account.
        Account account = CHILD_ACCOUNT;
        mAccountManager.addAccountHolderExplicitly(
                AccountHolder.create()
                        .account(account)
                        .addFeature(AccountManagerHelper.FEATURE_IS_CHILD_ACCOUNT_KEY)
                        .build());

        // Check the child account status with a callback and wait for it to resolve.
        HasChildAccountCallback callback = spy(new MockChildAccountCallback());
        mService.checkHasChildAccount(callback);
        Robolectric.idleMainLooper(0);

        verify(mService).onChildAccountStatusUpdated(null);
        verify(callback).onChildAccountChecked(true);

        // Checking the status again will call the callback, but not update the child account
        // status.
        HasChildAccountCallback callback2 = spy(new MockChildAccountCallback());
        mService.checkHasChildAccount(callback2);
        Robolectric.idleMainLooper(0);

        verify(callback2).onChildAccountChecked(true);
    }

    @Test
    public void testCachedHasChildAccount() {
        // If the child account status has not been determined, the native method will be called.
        // Using the inverted `do...().when()...` syntax to ensure that the non-overridden method
        // is not called.
        doReturn(true).when(mService).nativeGetIsChildAccount();

        assertTrue(mService.hasChildAccount());
    }

    @Test
    public void testAccountRemoved() {
        // Add one child account.
        AccountHolder accountHolder =
                AccountHolder.create()
                        .account(CHILD_ACCOUNT)
                        .addFeature(AccountManagerHelper.FEATURE_IS_CHILD_ACCOUNT_KEY)
                        .build();
        mAccountManager.addAccountHolderExplicitly(accountHolder);

        // Check the child account status with a callback and wait for it to resolve.
        HasChildAccountCallback callback = spy(new MockChildAccountCallback());
        mService.checkHasChildAccount(callback);
        Robolectric.idleMainLooper(0);

        verify(mService).onChildAccountStatusUpdated(null);
        verify(callback).onChildAccountChecked(true);

        // Removing the account and rechecking changes the child account status to false.
        mAccountManager.removeAccountHolderExplicitly(accountHolder);
        mService.recheckChildAccountStatus();

        verify(mService).onChildAccountStatusUpdated(true);
        assertFalse(mService.hasChildAccount());
    }

    @Test
    public void testAccountGraduated() {
        // Add one child account.
        AccountHolder accountHolder =
                AccountHolder.create()
                        .account(CHILD_ACCOUNT)
                        .addFeature(AccountManagerHelper.FEATURE_IS_CHILD_ACCOUNT_KEY)
                        .build();
        mAccountManager.addAccountHolderExplicitly(accountHolder);

        // Check the child account status with a callback and wait for it to resolve.
        HasChildAccountCallback callback = spy(new MockChildAccountCallback());
        mService.checkHasChildAccount(callback);
        Robolectric.idleMainLooper(0);

        verify(mService).onChildAccountStatusUpdated(null);
        verify(callback).onChildAccountChecked(true);

        // Replace the account holder with one where the account features are not fetched yet.
        mAccountManager.removeAccountHolderExplicitly(accountHolder);
        mAccountManager.addAccountHolderExplicitly(
                AccountHolder.create().account(CHILD_ACCOUNT).featureSet(null).build());
        mService.recheckChildAccountStatus();

        // While the check is still in progress, the child account status has its old value, and no
        // additional calls to onChildAccountStatusUpdated() were made.
        verify(mService).onChildAccountStatusUpdated(Matchers.any(Boolean.class));
        assertTrue(mService.hasChildAccount());

        // When the features are fetched, the child account status changes to false.
        mAccountManager.notifyFeaturesFetched(CHILD_ACCOUNT, new HashSet<String>());
        Robolectric.idleMainLooper(0);

        verify(mService).onChildAccountStatusUpdated(true);
        assertFalse(mService.hasChildAccount());
    }

    @Test
    public void testAccountRemovedDuringInitialCheck() {
        // Add one child account.
        AccountHolder accountHolder =
                AccountHolder.create().account(CHILD_ACCOUNT).featureSet(null).build();
        mAccountManager.addAccountHolderExplicitly(accountHolder);

        HasChildAccountCallback callback = spy(new MockChildAccountCallback());
        mService.checkHasChildAccount(callback);
        Robolectric.idleMainLooper(0);

        // The account features have not been fetched yet, so the child account status is
        // indeterminate.
        verify(mService, never()).onChildAccountStatusUpdated(Matchers.any(Boolean.class));
        verify(callback, never()).onChildAccountChecked(Matchers.anyBoolean());

        mAccountManager.removeAccountHolderExplicitly(accountHolder);
        mService.recheckChildAccountStatus();

        // When the account is removed, the child account status is set to false.
        verify(mService).onChildAccountStatusUpdated(null);
        verify(callback, never()).onChildAccountChecked(Matchers.anyBoolean());

        // Fetching the features for the removed account should not trigger a status update,
        // but the original callback will run.
        accountHolder.didFetchFeatures(
                Sets.newHashSet(AccountManagerHelper.FEATURE_IS_CHILD_ACCOUNT_KEY));
        Robolectric.idleMainLooper(0);

        verify(mService).onChildAccountStatusUpdated(Matchers.any(Boolean.class));
        verify(callback).onChildAccountChecked(false);
    }

    @Test
    public void testAccountGraduatedDuringInitialCheck() {
        // Add one child account.
        AccountHolder.Builder builder =
                AccountHolder.create().account(CHILD_ACCOUNT).featureSet(null);
        AccountHolder accountHolder = builder.build();
        mAccountManager.addAccountHolderExplicitly(accountHolder);

        HasChildAccountCallback callback = spy(new MockChildAccountCallback());
        mService.checkHasChildAccount(callback);
        Robolectric.idleMainLooper(0);

        // The account features have not been fetched yet, so the child account status is
        // undetermined.
        verify(mService, never()).onChildAccountStatusUpdated(Matchers.any(Boolean.class));
        verify(callback, never()).onChildAccountChecked(Matchers.anyBoolean());

        // Replace the account holder with a copy.
        mAccountManager.removeAccountHolderExplicitly(accountHolder);
        AccountHolder accountHolder2 = builder.build();
        mAccountManager.addAccountHolderExplicitly(accountHolder2);
        mService.recheckChildAccountStatus();

        // The child account status is still undetermined.
        verify(mService, never()).onChildAccountStatusUpdated(Matchers.any(Boolean.class));
        verify(callback, never()).onChildAccountChecked(Matchers.anyBoolean());

        // The first request for the account features returns, but the child account status stays
        // undetermined.
        accountHolder.didFetchFeatures(
                Sets.newHashSet(AccountManagerHelper.FEATURE_IS_CHILD_ACCOUNT_KEY));
        Robolectric.idleMainLooper(0);
        verify(mService, never()).onChildAccountStatusUpdated(Matchers.any(Boolean.class));
        verify(callback, never()).onChildAccountChecked(Matchers.anyBoolean());

        // Only when the second request for the account features returns, the child account status
        // is set to false.
        accountHolder2.didFetchFeatures(new HashSet<String>());
        Robolectric.idleMainLooper(0);

        verify(mService).onChildAccountStatusUpdated(null);
        verify(callback).onChildAccountChecked(false);
    }
}

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.accounts.Account;
import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Spy;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountTrackerService;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.test.util.MockSyncContentResolverDelegate;

import java.util.concurrent.atomic.AtomicInteger;

/** Tests for {@link SigninManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SigninManagerTest {
    /**
     * Overrides the native methods called during SigninManager construction.
     * Other native* methods can be overridden using Spy.
     */
    static class TestableSigninManager extends SigninManager {
        TestableSigninManager(Context context, AccountTrackerService accountTrackerService,
                AndroidSyncSettings androidSyncSettings) {
            super(context, accountTrackerService, androidSyncSettings);
        }

        @Override
        long nativeInit() {
            return 0;
        }

        @Override
        boolean nativeIsSigninAllowedByPolicy(long nativeSigninManagerAndroid) {
            return true;
        }
    }

    private AccountTrackerService mAccountTrackerService;
    @Spy
    private TestableSigninManager mSigninManager;

    @Before
    public void setUp() {
        mAccountTrackerService = mock(AccountTrackerService.class);
        AndroidSyncSettings androidSyncSettings = mock(AndroidSyncSettings.class);

        mSigninManager = spy(new TestableSigninManager(
                ContextUtils.getApplicationContext(), mAccountTrackerService, androidSyncSettings));

        // SinginManager interacts with AndroidSyncSettings, but its not the focus
        // of this test. Using MockSyncContentResolver reduces burden of test setup.
        AndroidSyncSettings.overrideForTests(new MockSyncContentResolverDelegate(), null);
    }

    @Test
    public void signOutFromJavaWithManagedDomain() {
        // Stub out various native calls. Some of these are verified as never called
        // and those stubs simply allow that verification to catch any issues.
        doNothing().when(mSigninManager).nativeSignOut(anyLong(), anyInt());
        doNothing().when(mSigninManager).nativeWipeProfileData(anyLong());
        doNothing().when(mSigninManager).nativeWipeGoogleServiceWorkerCaches(anyLong());
        // See verification of nativeWipeProfileData below.
        doReturn("TestDomain").when(mSigninManager).nativeGetManagementDomain(anyLong());

        // Trigger the sign out flow!
        mSigninManager.signOut(SignoutReason.SIGNOUT_TEST);

        // nativeSignOut should be called *before* clearing any account data.
        // http://crbug.com/589028
        verify(mSigninManager, times(1)).nativeSignOut(anyLong(), eq(SignoutReason.SIGNOUT_TEST));
        verify(mSigninManager, never()).nativeWipeGoogleServiceWorkerCaches(anyLong());
        verify(mSigninManager, never()).nativeWipeProfileData(anyLong());

        // Simulate native callback to trigger clearing of account data.
        mSigninManager.onNativeSignOut();

        // Sign-out should wipe all profile data when user has managed domain.
        verify(mSigninManager, times(1)).nativeWipeProfileData(anyLong());
        verify(mSigninManager, never()).nativeWipeGoogleServiceWorkerCaches(anyLong());
    }

    @Test
    public void signOutFromJavaWithNullDomain() {
        // Stub out various native calls. Some of these are verified as never called
        // and those stubs simply allow that verification to catch any issues.
        doNothing().when(mSigninManager).nativeSignOut(anyLong(), anyInt());
        doNothing().when(mSigninManager).nativeWipeProfileData(anyLong());
        doNothing().when(mSigninManager).nativeWipeGoogleServiceWorkerCaches(anyLong());
        // See verification of nativeWipeGoogleServiceWorkerCaches below.
        doReturn(null).when(mSigninManager).nativeGetManagementDomain(anyLong());

        // Trigger the sign out flow!
        mSigninManager.signOut(SignoutReason.SIGNOUT_TEST);

        // nativeSignOut should be called *before* clearing any account data.
        // http://crbug.com/589028
        verify(mSigninManager, times(1)).nativeSignOut(anyLong(), eq(SignoutReason.SIGNOUT_TEST));
        verify(mSigninManager, never()).nativeWipeGoogleServiceWorkerCaches(anyLong());
        verify(mSigninManager, never()).nativeWipeProfileData(anyLong());

        // Simulate native callback to trigger clearing of account data.
        mSigninManager.onNativeSignOut();

        // Sign-out should only clear the service worker cache when the domain is null.
        verify(mSigninManager, never()).nativeWipeProfileData(anyLong());
        verify(mSigninManager, times(1)).nativeWipeGoogleServiceWorkerCaches(anyLong());
    }

    @Test
    public void signOutFromNativeWithManagedDomain() {
        // Stub out various native calls. Some of these are verified as never called
        // and those stubs simply allow that verification to catch any issues.
        doNothing().when(mSigninManager).nativeSignOut(anyLong(), anyInt());
        doNothing().when(mSigninManager).nativeWipeProfileData(anyLong());
        doNothing().when(mSigninManager).nativeWipeGoogleServiceWorkerCaches(anyLong());
        // See verification of nativeWipeProfileData below.
        doReturn("TestDomain").when(mSigninManager).nativeGetManagementDomain(anyLong());

        // Trigger the sign out flow!
        mSigninManager.onNativeSignOut();

        // nativeSignOut should only be called when signOut() is triggered on
        // the Java side of the JNI boundary. This test instead initiates sign-out
        // from the native side.
        verify(mSigninManager, never()).nativeSignOut(anyLong(), anyInt());

        // Sign-out should wipe profile data when user has managed domain.
        verify(mSigninManager, times(1)).nativeWipeProfileData(anyLong());
        verify(mSigninManager, never()).nativeWipeGoogleServiceWorkerCaches(anyLong());
    }

    @Test
    public void signOutFromNativeWithNullDomain() {
        // Stub out various native calls. Some of these are verified as never called
        // and those stubs simply allow that verification to catch any issues.
        doNothing().when(mSigninManager).nativeSignOut(anyLong(), anyInt());
        doNothing().when(mSigninManager).nativeWipeProfileData(anyLong());
        doNothing().when(mSigninManager).nativeWipeGoogleServiceWorkerCaches(anyLong());
        // See verification of nativeWipeGoogleServiceWorkerCaches below.
        doReturn(null).when(mSigninManager).nativeGetManagementDomain(anyLong());

        // Trigger the sign out flow!
        mSigninManager.onNativeSignOut();

        // nativeSignOut should only be called when signOut() is triggered on
        // the Java side of the JNI boundary. This test instead initiates sign-out
        // from the native side.
        verify(mSigninManager, never()).nativeSignOut(anyLong(), anyInt());

        // Sign-out should only clear the service worker cache when the domain is null.
        verify(mSigninManager, never()).nativeWipeProfileData(anyLong());
        verify(mSigninManager, times(1)).nativeWipeGoogleServiceWorkerCaches(anyLong());
    }

    @Test
    public void callbackNotifiedWhenNoOperationIsInProgress() {
        assertFalse(mSigninManager.isOperationInProgress());

        AtomicInteger callCount = new AtomicInteger(0);
        mSigninManager.runAfterOperationInProgress(callCount::incrementAndGet);
        assertEquals(1, callCount.get());
    }

    @Test
    public void callbackNotifiedOnSignout() {
        doAnswer(invocation -> {
            mSigninManager.onNativeSignOut();
            return null;
        }).when(mSigninManager).nativeSignOut(anyLong(), anyInt());
        doReturn(null).when(mSigninManager).nativeGetManagementDomain(anyLong());
        doNothing().when(mSigninManager).nativeWipeGoogleServiceWorkerCaches(anyLong());

        mSigninManager.signOut(SignoutReason.SIGNOUT_TEST);
        assertTrue(mSigninManager.isOperationInProgress());
        AtomicInteger callCount = new AtomicInteger(0);
        mSigninManager.runAfterOperationInProgress(callCount::incrementAndGet);
        assertEquals(0, callCount.get());

        mSigninManager.onProfileDataWiped();
        assertFalse(mSigninManager.isOperationInProgress());
        assertEquals(1, callCount.get());
    }

    @Test
    public void callbackNotifiedOnSignin() {
        // No need to seed accounts to the native code.
        doReturn(true).when(mAccountTrackerService).checkAndSeedSystemAccounts();
        // Request that policy is loaded. It will pause sign-in until onPolicyCheckedBeforeSignIn is
        // invoked.
        doReturn(true).when(mSigninManager).nativeShouldLoadPolicyForUser(any());
        doNothing().when(mSigninManager).nativeCheckPolicyBeforeSignIn(anyLong(), any());

        doReturn(true).when(mSigninManager).isSigninSupported();
        doNothing().when(mSigninManager).nativeOnSignInCompleted(anyLong(), any());
        doNothing().when(mSigninManager).logInSignedInUser();

        mSigninManager.onFirstRunCheckDone(); // Allow sign-in.

        Account account = new Account("test@gmail.com", AccountManagerFacade.GOOGLE_ACCOUNT_TYPE);
        mSigninManager.signIn(account, null, null);
        assertTrue(mSigninManager.isOperationInProgress());
        AtomicInteger callCount = new AtomicInteger(0);
        mSigninManager.runAfterOperationInProgress(callCount::incrementAndGet);
        assertEquals(0, callCount.get());

        mSigninManager.onPolicyCheckedBeforeSignIn(null); // Test user is unmanaged.
        assertFalse(mSigninManager.isOperationInProgress());
        assertEquals(1, callCount.get());
    }
}

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.accounts.Account;
import android.app.Activity;
import android.app.Instrumentation;
import android.content.Context;
import android.content.Intent;
import android.support.test.filters.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.preferences.password.SavePasswordsPreferences;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.components.signin.AccountManagerHelper;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.MockAccountManager;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.test.util.MockSyncContentResolverDelegate;
import org.chromium.content.browser.test.NativeLibraryTestBase;

/**
 * Tests for verifying whether users are presented with the correct option of viewing
 * passwords according to the user group they belong to (syncing with sync passphrase,
 * syncing without sync passsphrase, non-syncing).
 */

public class PasswordViewingTypeTest extends NativeLibraryTestBase {

    private MainPreferences mMainPreferences;
    private ChromeBasePreference mPasswordsPref;
    private static final String DEFAULT_ACCOUNT = "test@gmail.com";
    private Context mContext;
    private MockSyncContentResolverDelegate mSyncContentResolverDelegate;
    private String mAuthority;
    private Account mAccount;
    private MockAccountManager mAccountManager;

    @Override
    protected void setUp() throws Exception {

        mSyncContentResolverDelegate = new MockSyncContentResolverDelegate();
        mContext = getInstrumentation().getTargetContext();
        mMainPreferences = (MainPreferences) startMainPreferences(getInstrumentation(),
            mContext).getFragmentForTest();
        mPasswordsPref = (ChromeBasePreference) mMainPreferences.findPreference(
                MainPreferences.PREF_SAVED_PASSWORDS);
        setupTestAccount(mContext);
        AndroidSyncSettings.overrideForTests(mContext, mSyncContentResolverDelegate);
        mAuthority = AndroidSyncSettings.getContractAuthority(mContext);
        AndroidSyncSettings.updateAccount(mContext, mAccount);
        super.setUp();
        loadNativeLibraryAndInitBrowserProcess();
    }

    private void setupTestAccount(Context context) {
        mAccountManager = new MockAccountManager(context, context);
        AccountManagerHelper.overrideAccountManagerHelperForTests(context, mAccountManager);
        mAccount = AccountManagerHelper.createAccountFromName("account@example.com");
        AccountHolder.Builder accountHolder =
                AccountHolder.create().account(mAccount).password("password").alwaysAccept(true);
        mAccountManager.addAccountHolderExplicitly(accountHolder.build());
    }

    /**
     * Launches the main preferences.
     */
    private static Preferences startMainPreferences(Instrumentation instrumentation,
            final Context mContext) {
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(mContext,
                MainPreferences.class.getName());
        Activity activity = (Preferences) instrumentation.startActivitySync(intent);
        return (Preferences) activity;
    }

    /**
     * Override ProfileSyncService using FakeProfileSyncService.
     */
    private void overrideProfileSyncService(final boolean usingPassphrase) {
        class FakeProfileSyncService extends ProfileSyncService {

            @Override
            public boolean isUsingSecondaryPassphrase() {
                return usingPassphrase;
            }

            @Override
            public boolean isEngineInitialized() {
                return true;
            }
        }
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ProfileSyncService.overrideForTests(new FakeProfileSyncService());
            }
        });
    }

    /**
     * Turn syncability on/off.
     */
    private void setSyncability(boolean syncState) throws InterruptedException {

        // Turn on syncability
        mSyncContentResolverDelegate.setMasterSyncAutomatically(syncState);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();

        // First sync
        mSyncContentResolverDelegate.setIsSyncable(mAccount, mAuthority, (syncState) ? 1 : 0);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();

        if (syncState) {
            mSyncContentResolverDelegate.setSyncAutomatically(mAccount, mAuthority, syncState);
            mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        }
    }

    /**
     * Verifies that sync settings are being set up correctly in the case of redirecting users.
     */
    @SmallTest
    @CommandLineFlags.Add("enable-features=" + MainPreferences.VIEW_PASSWORDS)
    @Feature({"Sync"})
    public void testUserRedirectSyncSettings() throws InterruptedException {
        setSyncability(true);
        overrideProfileSyncService(false);
        assertTrue(AndroidSyncSettings.isSyncEnabled(mContext));
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertTrue(ProfileSyncService.get().isEngineInitialized());
                assertFalse(ProfileSyncService.get().isUsingSecondaryPassphrase());
            }
        });
    }

    /**
     * Verifies that syncing users with a custom passphrase are allowed to
     * natively view passwords.
     */
    @SmallTest
    public void testSyncingNativePasswordView() throws InterruptedException {
        setSyncability(true);
        overrideProfileSyncService(true);
        assertEquals(SavePasswordsPreferences.class.getCanonicalName(),
                mPasswordsPref.getFragment());
        assertNotNull(mMainPreferences.getActivity().getIntent());
    }

    /**
     * Verifies that non-syncing users are allowed to natively view passwords.
     */
    @SmallTest
    public void testNonSyncingNativePasswordView() throws InterruptedException {
        setSyncability(false);
        assertEquals(SavePasswordsPreferences.class.getCanonicalName(),
                mPasswordsPref.getFragment());
    }

}

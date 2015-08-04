// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.invalidation;

import android.accounts.Account;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.test.InstrumentationTestCase;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.CollectionUtil;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.test.invalidation.IntentSavingContext;
import org.chromium.components.invalidation.InvalidationClientService;
import org.chromium.sync.AndroidSyncSettings;
import org.chromium.sync.internal_api.pub.base.ModelType;
import org.chromium.sync.notifier.InvalidationIntentProtocol;
import org.chromium.sync.signin.AccountManagerHelper;
import org.chromium.sync.signin.ChromeSigninController;
import org.chromium.sync.test.util.MockSyncContentResolverDelegate;

import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for the {@link InvalidationController}.
 */
public class InvalidationControllerTest extends InstrumentationTestCase {
    /**
     * Stubbed out ProfileSyncService with a setter to control return value of
     * {@link ProfileSyncService#getPreferredDataTypes()}
     */
    private static class ProfileSyncServiceStub extends ProfileSyncService {
        private Set<ModelType> mPreferredDataTypes;

        public ProfileSyncServiceStub(Context context) {
            super(context);
        }

        public void setPreferredDataTypes(Set<ModelType> types) {
            mPreferredDataTypes = types;
        }

        @Override
        protected void init(Context context) {
            // Skip native initialization.
        }

        @Override
        public Set<ModelType> getPreferredDataTypes() {
            return mPreferredDataTypes;
        }
    }

    private IntentSavingContext mContext;
    private InvalidationController mController;
    private ProfileSyncServiceStub mProfileSyncServiceStub;

    @Override
    protected void setUp() throws Exception {
        mContext = new IntentSavingContext(getInstrumentation().getTargetContext());
        mController = new InvalidationController(mContext, false);
        // We don't want to use the system content resolver, so we override it.
        MockSyncContentResolverDelegate delegate = new MockSyncContentResolverDelegate();
        // Android master sync can safely always be on.
        delegate.setMasterSyncAutomatically(true);
        AndroidSyncSettings.overrideForTests(mContext, delegate);
        mProfileSyncServiceStub = new ProfileSyncServiceStub(mContext);
        ProfileSyncService.overrideForTests(mProfileSyncServiceStub);
    }

    @SmallTest
    @Feature({"Sync"})
    public void testStop() throws Exception {
        mController.stop();
        assertEquals(1, mContext.getNumStartedIntents());
        Intent intent = mContext.getStartedIntent(0);
        validateIntentComponent(intent);
        assertEquals(1, intent.getExtras().size());
        assertTrue(intent.hasExtra(InvalidationIntentProtocol.EXTRA_STOP));
        assertTrue(intent.getBooleanExtra(InvalidationIntentProtocol.EXTRA_STOP, false));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testResumingMainActivity() throws Exception {
        // Resuming main activity should trigger a start if sync is enabled.
        setupSync(true);
        mController.onApplicationStateChange(ApplicationState.HAS_RUNNING_ACTIVITIES);
        assertEquals(1, mContext.getNumStartedIntents());
        Intent intent = mContext.getStartedIntent(0);
        validateIntentComponent(intent);
        assertFalse(intent.hasExtra(InvalidationIntentProtocol.EXTRA_STOP));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testResumingMainActivityWithSyncDisabled() throws Exception {
        // Resuming main activity should NOT trigger a start if sync is disabled.
        setupSync(false);
        mController.onApplicationStateChange(ApplicationState.HAS_RUNNING_ACTIVITIES);
        assertEquals(0, mContext.getNumStartedIntents());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testPausingMainActivity() throws Exception {
        // Resuming main activity should trigger a stop if sync is enabled.
        setupSync(true);
        mController.onApplicationStateChange(ApplicationState.HAS_PAUSED_ACTIVITIES);
        assertEquals(1, mContext.getNumStartedIntents());
        Intent intent = mContext.getStartedIntent(0);
        validateIntentComponent(intent);
        assertEquals(1, intent.getExtras().size());
        assertTrue(intent.hasExtra(InvalidationIntentProtocol.EXTRA_STOP));
        assertTrue(intent.getBooleanExtra(InvalidationIntentProtocol.EXTRA_STOP, false));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testPausingMainActivityWithSyncDisabled() throws Exception {
        // Resuming main activity should NOT trigger a stop if sync is disabled.
        setupSync(false);
        mController.onApplicationStateChange(ApplicationState.HAS_PAUSED_ACTIVITIES);
        assertEquals(0, mContext.getNumStartedIntents());
    }

    private void setupSync(boolean syncEnabled) {
        Account account = AccountManagerHelper.createAccountFromName("test@gmail.com");
        ChromeSigninController chromeSigninController = ChromeSigninController.get(mContext);
        chromeSigninController.setSignedInAccountName(account.name);
        if (syncEnabled) {
            AndroidSyncSettings.enableChromeSync(mContext);
        } else {
            AndroidSyncSettings.disableChromeSync(mContext);
        }
    }

    @SmallTest
    @Feature({"Sync"})
    public void testEnsureConstructorRegistersListener() throws Exception {
        final AtomicBoolean listenerCallbackCalled = new AtomicBoolean();

        // Create instance.
        new InvalidationController(mContext, false) {
            @Override
            public void onApplicationStateChange(int newState) {
                listenerCallbackCalled.set(true);
            }
        };

        // Ensure initial state is correct.
        assertFalse(listenerCallbackCalled.get());

        // Ensure we get a callback, which means we have registered for them.
        ApplicationStatus.onStateChangeForTesting(new Activity(), ActivityState.CREATED);
        assertTrue(listenerCallbackCalled.get());
    }

    @UiThreadTest
    @SmallTest
    @Feature({"Sync"})
    public void testEnsureStartedAndUpdateRegisteredTypes() {
        Account account = AccountManagerHelper.createAccountFromName("test@example.com");
        ChromeSigninController.get(mContext).setSignedInAccountName(account.name);
        mProfileSyncServiceStub.setPreferredDataTypes(
                CollectionUtil.newHashSet(ModelType.BOOKMARK, ModelType.SESSION));

        mController.ensureStartedAndUpdateRegisteredTypes();
        assertEquals(1, mContext.getNumStartedIntents());

        // Validate destination.
        Intent intent = mContext.getStartedIntent(0);
        validateIntentComponent(intent);
        assertEquals(InvalidationIntentProtocol.ACTION_REGISTER, intent.getAction());

        // Validate account.
        Account intentAccount =
                intent.getParcelableExtra(InvalidationIntentProtocol.EXTRA_ACCOUNT);
        assertEquals(account, intentAccount);

        // Validate registered types.
        Set<String> expectedTypes = CollectionUtil.newHashSet(ModelType.BOOKMARK.name(),
                ModelType.SESSION.name());
        Set<String> actualTypes = new HashSet<String>();
        actualTypes.addAll(intent.getStringArrayListExtra(
                                InvalidationIntentProtocol.EXTRA_REGISTERED_TYPES));
        assertEquals(expectedTypes, actualTypes);
        assertNull(InvalidationIntentProtocol.getRegisteredObjectIds(intent));
    }

    /**
     * Asserts that {@code intent} is destined for the correct component.
     */
    private static void validateIntentComponent(Intent intent) {
        assertNotNull(intent.getComponent());
        assertEquals(InvalidationClientService.class.getName(),
                intent.getComponent().getClassName());
    }

}

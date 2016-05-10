// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.invalidation;

import android.accounts.Account;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.test.ServiceTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import com.google.ipc.invalidation.external.client.InvalidationListener.RegistrationState;
import com.google.ipc.invalidation.external.client.contrib.AndroidListener;
import com.google.ipc.invalidation.external.client.types.ErrorInfo;
import com.google.ipc.invalidation.external.client.types.Invalidation;
import com.google.ipc.invalidation.external.client.types.ObjectId;

import org.chromium.base.CollectionUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.sync.ModelType;
import org.chromium.sync.ModelTypeHelper;
import org.chromium.sync.notifier.InvalidationIntentProtocol;
import org.chromium.sync.notifier.InvalidationPreferences;
import org.chromium.sync.notifier.InvalidationPreferences.EditContext;
import org.chromium.sync.signin.AccountManagerHelper;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Tests for the {@link InvalidationClientService}.
 *
 * @author dsmyers@google.com (Daniel Myers)
 */
public class InvalidationClientServiceTest extends
          ServiceTestCase<TestableInvalidationClientService> {
    /** Id used when creating clients. */
    private static final byte[] CLIENT_ID = new byte[]{0, 4, 7};
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "invalidation_test";

    /** Intents provided to {@link #startService}. */
    private List<Intent> mStartServiceIntents;

    public InvalidationClientServiceTest() {
        super(TestableInvalidationClientService.class);
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mStartServiceIntents = new ArrayList<Intent>();
        setContext(new AdvancedMockContext(getContext()) {
            @Override
            public ComponentName startService(Intent intent) {
                mStartServiceIntents.add(intent);
                return new ComponentName(this, InvalidationClientServiceTest.class);
            }
        });
        Context appContext = getContext().getApplicationContext();
        ContextUtils.initApplicationContextForTests(appContext);
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX, appContext);
        LibraryLoader.get(LibraryProcessType.PROCESS_BROWSER).ensureInitialized(getContext());
        setupService();
    }

    @Override
    public void tearDown() throws Exception {
        if (InvalidationClientService.getIsClientStartedForTest()) {
            Intent stopIntent = createStopIntent();
            getService().onHandleIntent(stopIntent);
        }
        assertFalse(InvalidationClientService.getIsClientStartedForTest());
        super.tearDown();
    }

    @SmallTest
    @Feature({"Sync"})
    public void testComputeRegistrationOps() {
        /*
         * Test plan: compute the set of registration operations resulting from various combinations
         * of existing and desired registrations. Verifying that they are correct.
         */
        Set<ObjectId> regAccumulator = new HashSet<ObjectId>();
        Set<ObjectId> unregAccumulator = new HashSet<ObjectId>();

        // Empty existing and desired registrations should yield empty operation sets.
        InvalidationClientService.computeRegistrationOps(
                toObjectIdSet(ModelType.BOOKMARKS, ModelType.SESSIONS),
                toObjectIdSet(ModelType.BOOKMARKS, ModelType.SESSIONS),
                regAccumulator, unregAccumulator);
        assertEquals(0, regAccumulator.size());
        assertEquals(0, unregAccumulator.size());

        // Equal existing and desired registrations should yield empty operation sets.
        InvalidationClientService.computeRegistrationOps(new HashSet<ObjectId>(),
                new HashSet<ObjectId>(), regAccumulator, unregAccumulator);
        assertEquals(0, regAccumulator.size());
        assertEquals(0, unregAccumulator.size());

        // Empty existing and non-empty desired registrations should yield desired registrations
        // as the registration operations to do and no unregistrations.
        Set<ObjectId> desiredTypes = toObjectIdSet(ModelType.BOOKMARKS, ModelType.SESSIONS);
        InvalidationClientService.computeRegistrationOps(
                new HashSet<ObjectId>(),
                desiredTypes,
                regAccumulator, unregAccumulator);
        assertEquals(
                toObjectIdSet(ModelType.BOOKMARKS, ModelType.SESSIONS),
                new HashSet<ObjectId>(regAccumulator));
        assertEquals(0, unregAccumulator.size());
        regAccumulator.clear();

        // Unequal existing and desired registrations should yield both registrations and
        // unregistrations. We should unregister TYPED_URLS and register BOOKMARKS, keeping
        // SESSIONS.
        InvalidationClientService.computeRegistrationOps(
                toObjectIdSet(ModelType.SESSIONS, ModelType.TYPED_URLS),
                toObjectIdSet(ModelType.BOOKMARKS, ModelType.SESSIONS),
                regAccumulator, unregAccumulator);
        assertEquals(toObjectIdSet(ModelType.BOOKMARKS), regAccumulator);
        assertEquals(toObjectIdSet(ModelType.TYPED_URLS), unregAccumulator);
        regAccumulator.clear();
        unregAccumulator.clear();
    }

    @SmallTest
    @Feature({"Sync"})
    public void testReady() {
       /**
        * Test plan: call ready. Verify that the service sets the client id correctly and reissues
        * pending registrations.
        */

        // Persist some registrations.
        InvalidationPreferences invPrefs = new InvalidationPreferences(getContext());
        EditContext editContext = invPrefs.edit();
        invPrefs.setSyncTypes(editContext, CollectionUtil.newArrayList("BOOKMARK", "SESSION"));
        ObjectId objectId = ObjectId.newInstance(1, "obj".getBytes());
        invPrefs.setObjectIds(editContext, CollectionUtil.newArrayList(objectId));
        assertTrue(invPrefs.commit(editContext));

        // Issue ready.
        getService().ready(CLIENT_ID);
        assertTrue(Arrays.equals(CLIENT_ID, InvalidationClientService.getClientIdForTest()));
        byte[] otherCid = "otherCid".getBytes();
        getService().ready(otherCid);
        assertTrue(Arrays.equals(otherCid, InvalidationClientService.getClientIdForTest()));

        // Verify registrations issued.
        assertEquals(CollectionUtil.newHashSet(
                toObjectId(ModelType.BOOKMARKS), toObjectId(ModelType.SESSIONS), objectId),
                new HashSet<ObjectId>(getService().mRegistrations.get(0)));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testReissueRegistrations() {
        /*
         * Test plan: call the reissueRegistrations method of the listener with both empty and
         * non-empty sets of desired registrations stored in preferences. Verify that no register
         * intent is set in the first case and that the appropriate register intent is sent in
         * the second.
         */

        // No persisted registrations.
        getService().reissueRegistrations(CLIENT_ID);
        assertTrue(getService().mRegistrations.isEmpty());

        // Persist some registrations.
        InvalidationPreferences invPrefs = new InvalidationPreferences(getContext());
        EditContext editContext = invPrefs.edit();
        invPrefs.setSyncTypes(editContext, CollectionUtil.newArrayList("BOOKMARK", "SESSION"));
        ObjectId objectId = ObjectId.newInstance(1, "obj".getBytes());
        invPrefs.setObjectIds(editContext, CollectionUtil.newArrayList(objectId));
        assertTrue(invPrefs.commit(editContext));

        // Reissue registrations and verify that the appropriate registrations are issued.
        getService().reissueRegistrations(CLIENT_ID);
        assertEquals(1, getService().mRegistrations.size());
        assertEquals(CollectionUtil.newHashSet(
                toObjectId(ModelType.BOOKMARKS), toObjectId(ModelType.SESSIONS), objectId),
                new HashSet<ObjectId>(getService().mRegistrations.get(0)));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testInformRegistrationStatus() {
        /*
         * Test plan: call inform registration status under a variety of circumstances and verify
         * that the appropriate (un)register calls are issued.
         *
         * 1. Registration of desired object. No calls issued.
         * 2. Unregistration of undesired object. No calls issued.
         * 3. Registration of undesired object. Unregistration issued.
         * 4. Unregistration of desired object. Registration issued.
         */
        // Initial test setup: persist a single registration into preferences.
        InvalidationPreferences invPrefs = new InvalidationPreferences(getContext());
        EditContext editContext = invPrefs.edit();
        invPrefs.setSyncTypes(editContext, CollectionUtil.newArrayList("SESSION"));
        ObjectId desiredObjectId = ObjectId.newInstance(1, "obj1".getBytes());
        ObjectId undesiredObjectId = ObjectId.newInstance(1, "obj2".getBytes());
        invPrefs.setObjectIds(editContext, CollectionUtil.newArrayList(desiredObjectId));
        assertTrue(invPrefs.commit(editContext));

        // Cases 1 and 2: calls matching desired state cause no actions.
        getService().informRegistrationStatus(CLIENT_ID, toObjectId(ModelType.SESSIONS),
                RegistrationState.REGISTERED);
        getService().informRegistrationStatus(CLIENT_ID, desiredObjectId,
                RegistrationState.REGISTERED);
        getService().informRegistrationStatus(CLIENT_ID, toObjectId(ModelType.BOOKMARKS),
                RegistrationState.UNREGISTERED);
        getService().informRegistrationStatus(CLIENT_ID, undesiredObjectId,
                RegistrationState.UNREGISTERED);
        assertTrue(getService().mRegistrations.isEmpty());
        assertTrue(getService().mUnregistrations.isEmpty());

        // Case 3: registration of undesired object triggers an unregistration.
        getService().informRegistrationStatus(CLIENT_ID, toObjectId(ModelType.BOOKMARKS),
                RegistrationState.REGISTERED);
        getService().informRegistrationStatus(CLIENT_ID, undesiredObjectId,
                RegistrationState.REGISTERED);
        assertEquals(2, getService().mUnregistrations.size());
        assertEquals(0, getService().mRegistrations.size());
        assertEquals(CollectionUtil.newArrayList(toObjectId(ModelType.BOOKMARKS)),
                getService().mUnregistrations.get(0));
        assertEquals(CollectionUtil.newArrayList(undesiredObjectId),
                getService().mUnregistrations.get(1));

        // Case 4: unregistration of a desired object triggers a registration.
        getService().informRegistrationStatus(CLIENT_ID, toObjectId(ModelType.SESSIONS),
                RegistrationState.UNREGISTERED);
        getService().informRegistrationStatus(CLIENT_ID, desiredObjectId,
                RegistrationState.UNREGISTERED);
        assertEquals(2, getService().mUnregistrations.size());
        assertEquals(2, getService().mRegistrations.size());
        assertEquals(CollectionUtil.newArrayList(toObjectId(ModelType.SESSIONS)),
                getService().mRegistrations.get(0));
        assertEquals(CollectionUtil.newArrayList(desiredObjectId),
                getService().mRegistrations.get(1));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testInformRegistrationFailure() {
        /*
         * Test plan: call inform registration failure under a variety of circumstances and verify
         * that the appropriate (un)register calls are issued.
         *
         * 1. Transient registration failure for an object that should be registered. Register
         *    should be called.
         * 2. Permanent registration failure for an object that should be registered. No calls.
         * 3. Transient registration failure for an object that should not be registered. Unregister
         *    should be called.
         * 4. Permanent registration failure for an object should not be registered. No calls.
         */

        // Initial test setup: persist a single registration into preferences.
        InvalidationPreferences invPrefs = new InvalidationPreferences(getContext());
        EditContext editContext = invPrefs.edit();
        invPrefs.setSyncTypes(editContext, CollectionUtil.newArrayList("SESSION"));
        ObjectId desiredObjectId = ObjectId.newInstance(1, "obj1".getBytes());
        ObjectId undesiredObjectId = ObjectId.newInstance(1, "obj2".getBytes());
        invPrefs.setObjectIds(editContext, CollectionUtil.newArrayList(desiredObjectId));
        assertTrue(invPrefs.commit(editContext));

        // Cases 2 and 4: permanent registration failures never cause calls to be made.
        getService().informRegistrationFailure(CLIENT_ID, toObjectId(ModelType.SESSIONS), false,
                "");
        getService().informRegistrationFailure(CLIENT_ID, toObjectId(ModelType.BOOKMARKS), false,
                "");
        getService().informRegistrationFailure(CLIENT_ID, desiredObjectId, false, "");
        getService().informRegistrationFailure(CLIENT_ID, undesiredObjectId, false, "");
        assertTrue(getService().mRegistrations.isEmpty());
        assertTrue(getService().mUnregistrations.isEmpty());

        // Case 1: transient failure of a desired registration results in re-registration.
        getService().informRegistrationFailure(CLIENT_ID, toObjectId(ModelType.SESSIONS), true, "");
        getService().informRegistrationFailure(CLIENT_ID, desiredObjectId, true, "");
        assertEquals(2, getService().mRegistrations.size());
        assertTrue(getService().mUnregistrations.isEmpty());
        assertEquals(CollectionUtil.newArrayList(toObjectId(ModelType.SESSIONS)),
                getService().mRegistrations.get(0));
        assertEquals(CollectionUtil.newArrayList(desiredObjectId),
                getService().mRegistrations.get(1));

        // Case 3: transient failure of an undesired registration results in unregistration.
        getService().informRegistrationFailure(CLIENT_ID, toObjectId(ModelType.BOOKMARKS), true,
                "");
        getService().informRegistrationFailure(CLIENT_ID, undesiredObjectId, true, "");
        assertEquals(2, getService().mRegistrations.size());
        assertEquals(2, getService().mUnregistrations.size());
        assertEquals(CollectionUtil.newArrayList(toObjectId(ModelType.BOOKMARKS)),
                getService().mUnregistrations.get(0));
        assertEquals(CollectionUtil.newArrayList(undesiredObjectId),
                getService().mUnregistrations.get(1));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testInformError() {
        /*
         * Test plan: call informError with both permanent and transient errors. Verify that
         * the transient error causes no action to be taken and that the permanent error causes
         * the client to be stopped.
         */

        // Client needs to be started for the permament error to trigger and stop.
        getService().setShouldRunStates(true, true);
        getService().onCreate();
        getService().onHandleIntent(createStartIntent());
        getService().mStartedServices.clear();  // Discard start intent.

        // Transient error.
        getService().informError(ErrorInfo.newInstance(0, true, "transient", null));
        assertTrue(getService().mStartedServices.isEmpty());

        // Permanent error.
        getService().informError(ErrorInfo.newInstance(0, false, "permanent", null));
        assertEquals(1, getService().mStartedServices.size());
        Intent sentIntent = getService().mStartedServices.get(0);
        Intent stopIntent = AndroidListener.createStopIntent(getContext());
        assertTrue(stopIntent.filterEquals(sentIntent));
        assertEquals(stopIntent.getExtras().keySet(), sentIntent.getExtras().keySet());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testReadWriteState() {
        /*
         * Test plan: read, write, and read the internal notification client persistent state.
         * Verify appropriate return values.
         */
        assertNull(getService().readState());
        byte[] writtenState = new byte[]{7, 4, 0};
        getService().writeState(writtenState);
        assertTrue(Arrays.equals(writtenState, getService().readState()));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testInvalidateWithPayload() {
        doTestInvalidate(true);
    }

    @SmallTest
    @Feature({"Sync"})
    public void testInvalidateWithoutPayload() {
        doTestInvalidate(false);
    }

    private void doTestInvalidate(boolean hasPayload) {
        /*
         * Test plan: call invalidate() with an invalidation that may or may not have a payload.
         * Verify the produced bundle has the correct fields.
         */
        // Call invalidate.
        int version = 4747;
        int objectSource = 55;
        String objectName = "BOOKMARK";
        ObjectId objectId = ObjectId.newInstance(objectSource, objectName.getBytes());
        final String payload = "testInvalidate-" + hasPayload;
        Invalidation invalidation = hasPayload
                ? Invalidation.newInstance(objectId, version, payload.getBytes())
                : Invalidation.newInstance(objectId, version);
        byte[] ackHandle = ("testInvalidate-" + hasPayload).getBytes();
        getService().invalidate(invalidation, ackHandle);

        // Validate bundle.
        assertEquals(1, getService().mRequestedSyncs.size());
        PendingInvalidation request = new PendingInvalidation(getService().mRequestedSyncs.get(0));
        assertEquals(objectSource, request.mObjectSource);
        assertEquals(objectName, request.mObjectId);
        assertEquals(version, request.mVersion);
        assertEquals(hasPayload ? payload : null, request.mPayload);

        // Ensure acknowledged.
        assertSingleAcknowledgement(ackHandle);
    }

    @SmallTest
    @Feature({"Sync"})
    public void testInvalidateUnknownVersion() {
        /*
         * Test plan: call invalidateUnknownVersion(). Verify the produced bundle has the correct
         * fields.
         */
        int objectSource = 55;
        String objectName = "BOOKMARK";
        ObjectId objectId = ObjectId.newInstance(objectSource, objectName.getBytes());
        byte[] ackHandle = "testInvalidateUV".getBytes();
        getService().invalidateUnknownVersion(objectId, ackHandle);

        // Validate bundle.
        assertEquals(1, getService().mRequestedSyncs.size());
        PendingInvalidation request = new PendingInvalidation(getService().mRequestedSyncs.get(0));
        assertEquals(objectSource, request.mObjectSource);
        assertEquals(objectName, request.mObjectId);
        assertEquals(0, request.mVersion);
        assertEquals(null, request.mPayload);

        // Ensure acknowledged.
        assertSingleAcknowledgement(ackHandle);
    }

    @SmallTest
    @Feature({"Sync"})
    public void testInvalidateAll() {
        /*
         * Test plan: call invalidateAll(). Verify the produced bundle has the correct fields.
         */
        byte[] ackHandle = "testInvalidateAll".getBytes();
        getService().invalidateAll(ackHandle);

        // Validate bundle.
        assertEquals(1, getService().mRequestedSyncs.size());
        PendingInvalidation request = new PendingInvalidation(getService().mRequestedSyncs.get(0));
        assertEquals(0, request.mObjectSource);

        // Ensure acknowledged.
        assertSingleAcknowledgement(ackHandle);
    }

    /** Asserts that the service received a single acknowledgement with handle {@code ackHandle}. */
    private void assertSingleAcknowledgement(byte[] ackHandle) {
        assertEquals(1, getService().mAcknowledgements.size());
        assertTrue(Arrays.equals(ackHandle, getService().mAcknowledgements.get(0)));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testShouldClientBeRunning() {
        /*
         * Test plan: call shouldClientBeRunning with various combinations of
         * in-foreground/sync-enabled. Verify appropriate return values.
         */
        getService().setShouldRunStates(false, false);
        assertFalse(getService().shouldClientBeRunning());

        getService().setShouldRunStates(false, true);
        assertFalse(getService().shouldClientBeRunning());

        getService().setShouldRunStates(true, false);
        assertFalse(getService().shouldClientBeRunning());

        // Should only be running if both in the foreground and sync is enabled.
        getService().setShouldRunStates(true, true);
        assertTrue(getService().shouldClientBeRunning());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testStartAndStopClient() {
        /*
         * Test plan: with Chrome configured so that the client should run, send it an empty
         * intent. Even though no owning account is known, the client should still start. Send
         * it a stop intent and verify that it stops.
         */

        // Note: we are manipulating the service object directly, rather than through startService,
        // because otherwise we would need to handle the asynchronous execution model of the
        // underlying IntentService.
        getService().setShouldRunStates(true, true);
        getService().onCreate();

        Intent startIntent = createStartIntent();
        getService().onHandleIntent(startIntent);
        assertTrue(InvalidationClientService.getIsClientStartedForTest());

        Intent stopIntent = createStopIntent();
        getService().onHandleIntent(stopIntent);
        assertFalse(InvalidationClientService.getIsClientStartedForTest());

        // The issued intents should have been an AndroidListener start intent followed by an
        // AndroidListener stop intent.
        assertEquals(2, mStartServiceIntents.size());
        assertTrue(isAndroidListenerStartIntent(mStartServiceIntents.get(0)));
        assertTrue(isAndroidListenerStopIntent(mStartServiceIntents.get(1)));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testClientStopsWhenShouldNotBeRunning() {
        /*
         * Test plan: start the client. Then, change the configuration so that Chrome should not
         * be running. Send an intent to the service and verify that it stops.
         */
        getService().setShouldRunStates(true, true);
        getService().onCreate();

        // Start the service.
        Intent startIntent = createStartIntent();
        getService().onHandleIntent(startIntent);
        assertTrue(InvalidationClientService.getIsClientStartedForTest());

        // Change configuration.
        getService().setShouldRunStates(false, false);

        // Send an Intent and verify that the service stops.
        getService().onHandleIntent(startIntent);
        assertFalse(InvalidationClientService.getIsClientStartedForTest());

        // The issued intents should have been an AndroidListener start intent followed by an
        // AndroidListener stop intent.
        assertEquals(2, mStartServiceIntents.size());
        assertTrue(isAndroidListenerStartIntent(mStartServiceIntents.get(0)));
        assertTrue(isAndroidListenerStopIntent(mStartServiceIntents.get(1)));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testRegistrationIntent() {
        /*
         * Test plan: send a registration-change intent. Verify that it starts the client and
         * sets both the account and registrations in shared preferences.
         */
        getService().setShouldRunStates(true, true);
        getService().onCreate();

        // Send register Intent.
        Set<Integer> desiredRegistrations = CollectionUtil.newHashSet(
                ModelType.BOOKMARKS, ModelType.SESSIONS);
        Account account = AccountManagerHelper.createAccountFromName("test@example.com");
        Intent registrationIntent = createRegisterIntent(account, desiredRegistrations);
        getService().onHandleIntent(registrationIntent);

        // Verify client started and state written.
        assertTrue(InvalidationClientService.getIsClientStartedForTest());
        InvalidationPreferences invPrefs = new InvalidationPreferences(getContext());
        assertEquals(account, invPrefs.getSavedSyncedAccount());
        assertEquals(modelTypesToNotificationTypes(desiredRegistrations),
                invPrefs.getSavedSyncedTypes());
        assertNull(invPrefs.getSavedObjectIds());
        assertEquals(1, mStartServiceIntents.size());
        assertTrue(isAndroidListenerStartIntent(mStartServiceIntents.get(0)));

        // Send a registration-change intent with different types to register for.
        desiredRegistrations = CollectionUtil.newHashSet(ModelType.PASSWORDS);
        getService().onHandleIntent(createRegisterIntent(account, desiredRegistrations));
        assertEquals(account, invPrefs.getSavedSyncedAccount());
        assertEquals(modelTypesToNotificationTypes(desiredRegistrations),
                invPrefs.getSavedSyncedTypes());
        assertEquals(1, mStartServiceIntents.size());

        // Finally, send one more registration-change intent, this time with a different account,
        // and verify that it both updates the account, stops the existing client, and
        // starts a new client.
        Account account2 = AccountManagerHelper.createAccountFromName("test2@example.com");
        getService().onHandleIntent(createRegisterIntent(account2, desiredRegistrations));
        assertEquals(account2, invPrefs.getSavedSyncedAccount());
        assertEquals(3, mStartServiceIntents.size());
        assertTrue(isAndroidListenerStartIntent(mStartServiceIntents.get(0)));
        assertTrue(isAndroidListenerStopIntent(mStartServiceIntents.get(1)));
        assertTrue(isAndroidListenerStartIntent(mStartServiceIntents.get(2)));
    }

    /**
     * Determines if the correct object ids have been written to preferences and registered with the
     * invalidation client.
     *
     * @param expectedTypes The Sync types expected to be registered.
     * @param expectedObjectIds The additional object ids expected to be registered.
     * @param isReady Whether the client is ready to register/unregister.
     */
    private boolean expectedObjectIdsRegistered(Set<Integer> expectedTypes,
            Set<ObjectId> expectedObjectIds, boolean isReady) {
        // Get synced types saved to preferences.
        Set<String> expectedSyncTypes = modelTypesToNotificationTypes(expectedTypes);
        InvalidationPreferences invPrefs = new InvalidationPreferences(getContext());
        Set<String> actualSyncTypes = invPrefs.getSavedSyncedTypes();
        if (actualSyncTypes == null) {
            actualSyncTypes = new HashSet<String>();
        }

        // Get object ids saved to preferences.
        Set<ObjectId> actualObjectIds = invPrefs.getSavedObjectIds();
        if (actualObjectIds == null) {
            actualObjectIds = new HashSet<ObjectId>();
        }

        // Get expected registered object ids.
        Set<ObjectId> expectedRegisteredIds = new HashSet<ObjectId>();
        if (isReady) {
            expectedRegisteredIds.addAll(modelTypesToObjectIds(expectedTypes));
            expectedRegisteredIds.addAll(expectedObjectIds);
        }

        return actualSyncTypes.equals(expectedSyncTypes)
                && actualObjectIds.equals(expectedObjectIds)
                && getService().mCurrentRegistrations.equals(expectedRegisteredIds);
    }

    @SmallTest
    @Feature({"Sync"})
    public void testRegistrationIntentWithTypesAndObjectIds() {
        /*
         * Test plan: send a mix of registration-change intents: some for Sync types and some for
         * object ids. Verify that registering for Sync types does not interfere with object id
         * registration and vice-versa.
         */
        getService().setShouldRunStates(true, true);
        getService().onCreate();

        Account account = AccountManagerHelper.createAccountFromName("test@example.com");
        Set<ObjectId> objectIds = new HashSet<ObjectId>();
        Set<Integer> types = new HashSet<Integer>();

        // Register for some object ids.
        objectIds.add(ObjectId.newInstance(1, "obj1".getBytes()));
        objectIds.add(ObjectId.newInstance(2, "obj2".getBytes()));
        Intent registrationIntent =
                createRegisterIntent(account, new int[] {1, 2}, new String[] {"obj1", "obj2"});
        getService().onHandleIntent(registrationIntent);
        assertTrue(expectedObjectIdsRegistered(types, objectIds, false /* isReady */));

        // Register for some types.
        types.add(ModelType.BOOKMARKS);
        types.add(ModelType.SESSIONS);
        registrationIntent = createRegisterIntent(account, types);
        getService().onHandleIntent(registrationIntent);
        assertTrue(expectedObjectIdsRegistered(types, objectIds, false /* isReady */));

        // Set client to be ready and verify registrations.
        getService().ready(CLIENT_ID);
        assertTrue(expectedObjectIdsRegistered(types, objectIds, true /* isReady */));

        // Change object id registration with types registered.
        objectIds.add(ObjectId.newInstance(3, "obj3".getBytes()));
        registrationIntent = createRegisterIntent(
            account, new int[] {1, 2, 3}, new String[] {"obj1", "obj2", "obj3"});
        getService().onHandleIntent(registrationIntent);
        assertTrue(expectedObjectIdsRegistered(types, objectIds, true /* isReady */));

        // Change type registration with object ids registered.
        types.remove(ModelType.BOOKMARKS);
        registrationIntent = createRegisterIntent(account, types);
        getService().onHandleIntent(registrationIntent);
        assertTrue(expectedObjectIdsRegistered(types, objectIds, true /* isReady */));

        // Unregister all types.
        types.clear();
        registrationIntent = createRegisterIntent(account, types);
        getService().onHandleIntent(registrationIntent);
        assertTrue(expectedObjectIdsRegistered(types, objectIds, true /* isReady */));

        // Change object id registration with no types registered.
        objectIds.remove(ObjectId.newInstance(2, "obj2".getBytes()));
        registrationIntent = createRegisterIntent(
            account, new int[] {1, 3}, new String[] {"obj1", "obj3"});
        getService().onHandleIntent(registrationIntent);
        assertTrue(expectedObjectIdsRegistered(types, objectIds, true /* isReady */));

        // Unregister all object ids.
        objectIds.clear();
        registrationIntent = createRegisterIntent(account, new int[0], new String[0]);
        getService().onHandleIntent(registrationIntent);
        assertTrue(expectedObjectIdsRegistered(types, objectIds, true /* isReady */));

        // Change type registration with no object ids registered.
        types.add(ModelType.BOOKMARKS);
        types.add(ModelType.PASSWORDS);
        registrationIntent = createRegisterIntent(account, types);
        getService().onHandleIntent(registrationIntent);
        assertTrue(expectedObjectIdsRegistered(types, objectIds, true /* isReady */));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testRegistrationIntentNoProxyTabsUsingReady() {
        getService().setShouldRunStates(true, true);
        getService().onCreate();

        // Send register Intent.
        Account account = AccountManagerHelper.createAccountFromName("test@example.com");
        Intent registrationIntent = createRegisterIntent(
                account, CollectionUtil.newHashSet(ModelType.PROXY_TABS, ModelType.SESSIONS));
        getService().onHandleIntent(registrationIntent);

        // Verify client started and state written.
        assertTrue(InvalidationClientService.getIsClientStartedForTest());
        InvalidationPreferences invPrefs = new InvalidationPreferences(getContext());
        assertEquals(account, invPrefs.getSavedSyncedAccount());
        assertEquals(
                CollectionUtil.newHashSet("PROXY_TABS", "SESSION"), invPrefs.getSavedSyncedTypes());
        assertEquals(1, mStartServiceIntents.size());
        assertTrue(isAndroidListenerStartIntent(mStartServiceIntents.get(0)));

        // Set client to be ready. This triggers registrations.
        getService().ready(CLIENT_ID);
        assertTrue(Arrays.equals(CLIENT_ID, InvalidationClientService.getClientIdForTest()));

        // Ensure registrations are correct.
        Set<ObjectId> expectedRegistrations =
                modelTypesToObjectIds(CollectionUtil.newHashSet(ModelType.SESSIONS));
        assertEquals(expectedRegistrations,
                     new HashSet<ObjectId>(getService().mRegistrations.get(0)));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testRegistrationIntentNoProxyTabsAlreadyWithClientId() {
        getService().setShouldRunStates(true, true);
        getService().onCreate();

        // Send register Intent with no desired types.
        Account account = AccountManagerHelper.createAccountFromName("test@example.com");
        Intent registrationIntent = createRegisterIntent(account, new HashSet<Integer>());
        getService().onHandleIntent(registrationIntent);

        // Verify client started and state written.
        assertTrue(InvalidationClientService.getIsClientStartedForTest());
        InvalidationPreferences invPrefs = new InvalidationPreferences(getContext());
        assertEquals(account, invPrefs.getSavedSyncedAccount());
        assertEquals(new HashSet<String>(), invPrefs.getSavedSyncedTypes());
        assertEquals(1, mStartServiceIntents.size());
        assertTrue(isAndroidListenerStartIntent(mStartServiceIntents.get(0)));

        // Make sure client is ready.
        getService().ready(CLIENT_ID);
        assertTrue(Arrays.equals(CLIENT_ID, InvalidationClientService.getClientIdForTest()));

        // Send register Intent for SESSIONS and PROXY_TABS in an already ready client.
        registrationIntent = createRegisterIntent(account,
                CollectionUtil.newHashSet(ModelType.PROXY_TABS, ModelType.SESSIONS));
        getService().onHandleIntent(registrationIntent);

        // Ensure that PROXY_TABS registration request is ignored.
        assertEquals(1, getService().mRegistrations.size());
        Set<ObjectId> expectedTypes =
                modelTypesToObjectIds(CollectionUtil.newHashSet(ModelType.SESSIONS));
        assertEquals(expectedTypes, new HashSet<ObjectId>(getService().mRegistrations.get(0)));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testRegistrationIntentWhenClientShouldNotBeRunning() {
        /*
         * Test plan: send a registration change event when the client should not be running.
         * Verify that the service updates the on-disk state but does not start the client.
         */
        getService().onCreate();

        // Send register Intent.
        Account account = AccountManagerHelper.createAccountFromName("test@example.com");
        Set<Integer> desiredRegistrations = CollectionUtil.newHashSet(
                ModelType.BOOKMARKS, ModelType.SESSIONS);
        Intent registrationIntent = createRegisterIntent(account, desiredRegistrations);
        getService().onHandleIntent(registrationIntent);

        // Verify state written but client not started.
        assertFalse(InvalidationClientService.getIsClientStartedForTest());
        InvalidationPreferences invPrefs = new InvalidationPreferences(getContext());
        assertEquals(account, invPrefs.getSavedSyncedAccount());
        assertEquals(modelTypesToNotificationTypes(desiredRegistrations),
                invPrefs.getSavedSyncedTypes());
        assertEquals(0, mStartServiceIntents.size());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testDeferredRegistrationsIssued() {
        /*
         * Test plan: send a registration-change intent. Verify that the client issues a start
         * intent but makes no registration calls. Issue a reissueRegistrations call and verify
         * that the client does issue the appropriate registrations.
         */
        getService().setShouldRunStates(true, true);
        getService().onCreate();

        // Send register Intent. Verify client started but no registrations issued.
        Account account = AccountManagerHelper.createAccountFromName("test@example.com");
        Set<Integer> desiredRegistrations = CollectionUtil.newHashSet(
                ModelType.BOOKMARKS, ModelType.SESSIONS);
        Set<ObjectId> desiredObjectIds = modelTypesToObjectIds(desiredRegistrations);

        Intent registrationIntent = createRegisterIntent(account, desiredRegistrations);
        getService().onHandleIntent(registrationIntent);
        assertTrue(InvalidationClientService.getIsClientStartedForTest());
        assertEquals(1, mStartServiceIntents.size());
        assertTrue(isAndroidListenerStartIntent(mStartServiceIntents.get(0)));
        InvalidationPreferences invPrefs = new InvalidationPreferences(getContext());
        assertEquals(modelTypesToNotificationTypes(desiredRegistrations),
                invPrefs.getSavedSyncedTypes());
        assertEquals(desiredObjectIds, getService().readRegistrationsFromPrefs());

        // Issue reissueRegistrations; verify registration intent issues.
        getService().reissueRegistrations(CLIENT_ID);
        assertEquals(2, mStartServiceIntents.size());
        Intent expectedRegisterIntent = AndroidListener.createRegisterIntent(
                getContext(),
                CLIENT_ID,
                desiredObjectIds);
        Intent actualRegisterIntent = mStartServiceIntents.get(1);
        assertTrue(expectedRegisterIntent.filterEquals(actualRegisterIntent));
        assertEquals(expectedRegisterIntent.getExtras().keySet(),
                actualRegisterIntent.getExtras().keySet());
        assertEquals(
                desiredObjectIds,
                new HashSet<ObjectId>(getService().mRegistrations.get(0)));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testNullIntent() {
        getService().setShouldRunStates(true, true);
        getService().onCreate();
        // onHandleIntent must gracefully handle receiving a null intent.
        getService().onHandleIntent(null);
        // No crash == success.
    }

    @SmallTest
    @Feature({"Sync"})
    public void testRegistrationRetries() {
        /*
         * Test plan: validate that the alarm receiver used by the AndroidListener underlying
         * InvalidationClientService is correctly configured in the manifest and retries
         * registrations with exponential backoff. May need to be implemented as a downstream
         * Chrome for Android test.
         */
        // TODO(dsmyers): implement.
        // Bug: https://code.google.com/p/chromium/issues/detail?id=172398
    }

    private ObjectId toObjectId(int modelType) {
        return ModelTypeHelper.toObjectId(modelType);
    }

    private Set<ObjectId> toObjectIdSet(int... modelTypes) {
        Set<ObjectId> objectIds = new HashSet<ObjectId>(modelTypes.length);
        for (int i = 0; i < modelTypes.length; i++) {
            objectIds.add(toObjectId(modelTypes[i]));
        }
        return objectIds;
    }

    private Set<ObjectId> modelTypesToObjectIds(Set<Integer> modelTypes) {
        Set<ObjectId> objectIds = new HashSet<ObjectId>();
        for (Integer modelType : modelTypes) {
            objectIds.add(toObjectId(modelType));
        }
        return objectIds;
    }

    private Set<String> modelTypesToNotificationTypes(Set<Integer> modelTypes) {
        Set<String> strings = new HashSet<String>();
        for (Integer modelType : modelTypes) {
            strings.add(ModelTypeHelper.toNotificationType(modelType));
        }
        return strings;
    }

    /** Creates an intent to start the InvalidationClientService. */
    private Intent createStartIntent() {
        Intent intent = new Intent();
        return intent;
    }

    /** Creates an intent to stop the InvalidationClientService. */
    private Intent createStopIntent() {
        Intent intent = new Intent();
        intent.putExtra(InvalidationIntentProtocol.EXTRA_STOP, true);
        return intent;
    }

    /** Creates an intent to register some types with the InvalidationClientService. */
    private Intent createRegisterIntent(Account account, Set<Integer> types) {
        Intent intent = InvalidationIntentProtocol.createRegisterIntent(account, types);
        return intent;
    }

    /** Creates an intent to register some types with the InvalidationClientService. */
    private Intent createRegisterIntent(
            Account account, int[] objectSources, String[] objectNames) {
        Intent intent = InvalidationIntentProtocol.createRegisterIntent(
                account, objectSources, objectNames);
        return intent;
    }

    /** Returns whether {@code intent} is an {@link AndroidListener} start intent. */
    private boolean isAndroidListenerStartIntent(Intent intent) {
        Intent startIntent = AndroidListener.createStartIntent(getContext(),
                InvalidationClientService.CLIENT_TYPE, "unused".getBytes());
        return intent.getExtras().keySet().equals(startIntent.getExtras().keySet());
    }

    /** Returns whether {@code intent} is an {@link AndroidListener} stop intent. */
    private boolean isAndroidListenerStopIntent(Intent intent) {
        Intent stopIntent = AndroidListener.createStopIntent(getContext());
        return intent.getExtras().keySet().equals(stopIntent.getExtras().keySet());
    }
}

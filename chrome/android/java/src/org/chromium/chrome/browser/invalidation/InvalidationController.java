// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.invalidation;

import android.accounts.Account;
import android.content.Context;
import android.content.Intent;
import android.util.Base64;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Preconditions;

import org.chromium.base.ActivityStatus;
import org.chromium.base.CalledByNative;
import org.chromium.chrome.browser.identity.UniqueIdentificationGeneratorFactory;
import org.chromium.chrome.browser.identity.UniqueIdentificationGenerator;
import org.chromium.sync.internal_api.pub.base.ModelType;
import org.chromium.sync.notifier.InvalidationIntentProtocol;
import org.chromium.sync.notifier.InvalidationPreferences;
import org.chromium.sync.notifier.InvalidationService;
import org.chromium.sync.notifier.SyncStatusHelper;

import java.util.Random;
import java.util.Set;

/**
 * Controller used to send start, stop, and registration-change commands to the invalidation
 * client library used by Sync.
 */
public class InvalidationController implements ActivityStatus.StateListener {
    private static final Object LOCK = new Object();

    // Key to use when initializing the UniqueIdentificationGeneratorFactory's entry for this class.
    public static final String ID_GENERATOR = "INVALIDATION_CONTROLLER_ID_GENERATOR";

    // Pref key to use for UUID-based generator.
    public static final String INVALIDATIONS_UUID_PREF_KEY = "chromium.invalidations.uuid";

    private static final Random RANDOM = new Random();

    private static InvalidationController sInstance;

    private final Object mLock = new Object();

    private final Context mContext;

    private byte[] mUniqueId;

    /**
     * Sets the types for which the client should register for notifications.
     *
     * @param account  Account of the user.
     * @param allTypes If {@code true}, registers for all types, and {@code types} is ignored
     * @param types    Set of types for which to register. Ignored if {@code allTypes == true}.
     */
    public void setRegisteredTypes(Account account, boolean allTypes, Set<ModelType> types) {
        Intent registerIntent =
                InvalidationIntentProtocol.createRegisterIntent(account, allTypes, types);
        registerIntent.setClass(mContext, InvalidationService.class);
        registerIntent.putExtra(
                InvalidationIntentProtocol.EXTRA_CLIENT_NAME, getInvalidatorClientId());
        mContext.startService(registerIntent);
    }

    /**
     * Reads all stored preferences and calls
     * {@link #setRegisteredTypes(android.accounts.Account, boolean, java.util.Set)} with the stored
     * values, refreshing the set of types with {@code types}. It can be used on startup of Chrome
     * to ensure we always have a set of registrations consistent with the native code.
     * @param types    Set of types for which to register.
     */
    public void refreshRegisteredTypes(Set<ModelType> types) {
        InvalidationPreferences invalidationPreferences = new InvalidationPreferences(mContext);
        Set<String> savedSyncedTypes = invalidationPreferences.getSavedSyncedTypes();
        Account account = invalidationPreferences.getSavedSyncedAccount();
        boolean allTypes = savedSyncedTypes != null &&
                savedSyncedTypes.contains(ModelType.ALL_TYPES_TYPE);
        setRegisteredTypes(account, allTypes, types);
    }

    /**
     * Sets object ids for which the client should register for notification. This is intended for
     * registering non-Sync types; Sync types are registered with {@code setRegisteredTypes}.
     *
     * @param objectSources The sources of the objects.
     * @param objectNames   The names of the objects.
     */
    @CalledByNative
    public void setRegisteredObjectIds(int[] objectSources, String[] objectNames) {
        InvalidationPreferences invalidationPreferences = new InvalidationPreferences(mContext);
        Account account = invalidationPreferences.getSavedSyncedAccount();
        Intent registerIntent =
                InvalidationIntentProtocol.createRegisterIntent(
                        account, objectSources, objectNames);
        registerIntent.setClass(mContext, InvalidationService.class);
        registerIntent.putExtra(
            InvalidationIntentProtocol.EXTRA_CLIENT_NAME, getInvalidatorClientId());
        mContext.startService(registerIntent);
    }

    /**
     * Starts the invalidation client.
     */
    public void start() {
        Intent intent = new Intent(mContext, InvalidationService.class);
        intent.putExtra(
            InvalidationIntentProtocol.EXTRA_CLIENT_NAME, getInvalidatorClientId());
        mContext.startService(intent);
    }

    /**
     * Stops the invalidation client.
     */
    public void stop() {
        Intent intent = new Intent(mContext, InvalidationService.class);
        intent.putExtra(InvalidationIntentProtocol.EXTRA_STOP, true);
        intent.putExtra(
            InvalidationIntentProtocol.EXTRA_CLIENT_NAME, getInvalidatorClientId());
        mContext.startService(intent);
    }

    /**
     * Returns the instance that will use {@code context} to issue intents.
     *
     * Calling this method will create the instance if it does not yet exist.
     */
    @CalledByNative
    public static InvalidationController get(Context context) {
        synchronized (LOCK) {
            if (sInstance == null) {
                sInstance = new InvalidationController(context);
            }
            return sInstance;
        }
    }

    /**
     * Creates an instance using {@code context} to send intents.
     */
    @VisibleForTesting
    InvalidationController(Context context) {
        mContext = Preconditions.checkNotNull(context.getApplicationContext());
        ActivityStatus.registerStateListener(this);
    }

    @Override
    public void onActivityStateChange(int newState) {
        if (SyncStatusHelper.get(mContext).isSyncEnabled()) {
            if (newState == ActivityStatus.PAUSED) {
                stop();
            } else if (newState == ActivityStatus.RESUMED) {
                start();
            }
        }
    }

    @CalledByNative
    public byte[] getInvalidatorClientId() {
        synchronized(mLock) {
            if (mUniqueId != null) {
                return mUniqueId;
            }

            try {
                UniqueIdentificationGenerator generator =
                    UniqueIdentificationGeneratorFactory.getInstance(ID_GENERATOR);
                mUniqueId = generator.getUniqueId(null).getBytes();
            } catch (RuntimeException e) {
                // Tests won't have a generator available.  We need to handle them differently.  But
                // it would be dangerous to return a hard-coded string, since that could break
                // invalidations if it was ever called in a real browser instance.
                //
                // A randomly generated ID is less harmful.  We'll add a "BadID" prefix to it so
                // hopefully someone will notice and file a bug if we ever used it in practice.
                byte[] randomBytes = new byte[8];
                RANDOM.nextBytes(randomBytes);
                String encoded =
                    Base64.encodeToString(randomBytes, 0, randomBytes.length, Base64.NO_WRAP);
                String idString = "BadID" + encoded;
                mUniqueId = idString.getBytes();
            }
            return mUniqueId;
        }
    }
}

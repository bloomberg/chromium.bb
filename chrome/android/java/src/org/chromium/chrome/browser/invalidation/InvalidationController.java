// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.invalidation;

import android.content.Context;
import android.content.Intent;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.FieldTrialList;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.components.invalidation.InvalidationClientService;
import org.chromium.sync.AndroidSyncSettings;
import org.chromium.sync.internal_api.pub.base.ModelType;
import org.chromium.sync.notifier.InvalidationIntentProtocol;
import org.chromium.sync.signin.ChromeSigninController;

import java.util.HashSet;

/**
 * Controller used to send start, stop, and registration-change commands to the invalidation
 * client library used by Sync.
 */
public class InvalidationController implements ApplicationStatus.ApplicationStateListener {
    private static final Object LOCK = new Object();

    private static InvalidationController sInstance;

    private final Context mContext;

    /**
     * Whether session sync invalidations should be disabled.
     */
    private final boolean mDisableSessionInvalidations;

    /**
     * Updates the sync invalidation types that the client is registered for based on the preferred
     * sync types.  Starts the client if needed.
     */
    public void ensureStartedAndUpdateRegisteredTypes() {
        HashSet<ModelType> typesToRegister = new HashSet<ModelType>();
        typesToRegister.addAll(ProfileSyncService.get(mContext).getPreferredDataTypes());
        if (mDisableSessionInvalidations) {
            typesToRegister.remove(ModelType.SESSION);
            typesToRegister.remove(ModelType.FAVICON_TRACKING);
            typesToRegister.remove(ModelType.FAVICON_IMAGE);
        }

        Intent registerIntent = InvalidationIntentProtocol.createRegisterIntent(
                ChromeSigninController.get(mContext).getSignedInUser(),
                typesToRegister);
        registerIntent.setClass(mContext, InvalidationClientService.class);
        mContext.startService(registerIntent);
    }

    /**
     * Starts the invalidation client without updating the registered invalidation types.
     */
    private void start() {
        Intent intent = new Intent(mContext, InvalidationClientService.class);
        mContext.startService(intent);
    }

    /**
     * Stops the invalidation client.
     */
    public void stop() {
        Intent intent = new Intent(mContext, InvalidationClientService.class);
        intent.putExtra(InvalidationIntentProtocol.EXTRA_STOP, true);
        mContext.startService(intent);
    }

    /**
     * Returns the instance that will use {@code context} to issue intents.
     *
     * Calling this method will create the instance if it does not yet exist.
     */
    public static InvalidationController get(Context context) {
        synchronized (LOCK) {
            if (sInstance == null) {
                boolean disableSessionInvalidations =
                        FieldTrialList.findFullName("AndroidSessionNotifications")
                                .equals("Disabled");
                sInstance = new InvalidationController(context, disableSessionInvalidations);
            }
            return sInstance;
        }
    }

    /**
     * Creates an instance using {@code context} to send intents.
     */
    @VisibleForTesting
    InvalidationController(Context context, boolean disableSessionInvalidations) {
        Context appContext = context.getApplicationContext();
        if (appContext == null) throw new NullPointerException("Unable to get application context");
        mContext = appContext;
        mDisableSessionInvalidations = disableSessionInvalidations;
        ApplicationStatus.registerApplicationStateListener(this);
    }

    @Override
    public void onApplicationStateChange(int newState) {
        if (AndroidSyncSettings.isSyncEnabled(mContext)) {
            if (newState == ApplicationState.HAS_RUNNING_ACTIVITIES) {
                start();
            } else if (newState == ApplicationState.HAS_PAUSED_ACTIVITIES) {
                stop();
            }
        }
    }
}

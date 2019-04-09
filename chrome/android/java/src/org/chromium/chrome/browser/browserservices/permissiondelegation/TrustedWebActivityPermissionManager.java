// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.preferences.website.ContentSettingValues;

import java.util.ArrayList;
import java.util.List;

import javax.inject.Inject;
import javax.inject.Singleton;

import dagger.Lazy;

/**
 * Handles the preserving and surfacing the permissions of TWA client apps for their associated
 * websites. Communicates with the {@link TrustedWebActivityPermissionStore} and
 * {@link InstalledWebappBridge}.
 *
 * Lifecycle: This is a singleton.
 * Thread safety: Only call methods on a single thread.
 * Native: Does not require native.
 */
@Singleton
public class TrustedWebActivityPermissionManager {
    private static final String TAG = "TwaPermissionManager";

    private final TrustedWebActivityPermissionStore mStore;

    // Use a Lazy instance so we don't instantiate it on Android versions pre-O.
    private final Lazy<NotificationChannelPreserver> mPermissionPreserver;

    @Inject
    public TrustedWebActivityPermissionManager(TrustedWebActivityPermissionStore store,
            Lazy<NotificationChannelPreserver> preserver) {
        mStore = store;
        mPermissionPreserver = preserver;
    }

    InstalledWebappBridge.Permission[] getNotificationPermissions() {
        List<InstalledWebappBridge.Permission> permissions = new ArrayList<>();
        for (String originAsString : mStore.getStoredOrigins()) {
            Origin origin = new Origin(originAsString);
            Boolean enabled = mStore.areNotificationsEnabled(origin);

            if (enabled == null) {
                Log.w(TAG, "%s is known but has no notification permission.", origin);
                continue;
            }

            @ContentSettingValues
            int setting = enabled ? ContentSettingValues.ALLOW : ContentSettingValues.BLOCK;

            permissions.add(new InstalledWebappBridge.Permission(origin, setting));
        }

        return permissions.toArray(new InstalledWebappBridge.Permission[permissions.size()]);
    }

    void register(Origin origin, boolean notificationsEnabled) {
        // TODO: Only trigger if this is for the first time?
        NotificationChannelPreserver.deleteChannelIfNeeded(mPermissionPreserver, origin);

        mStore.setNotificationState(origin, notificationsEnabled);
    }

    void unregister(Origin origin) {
        NotificationChannelPreserver.restoreChannelIfNeeded(mPermissionPreserver, origin);

        mStore.removeOrigin(origin);
    }

    @VisibleForTesting
    void clearForTesting() {
        mStore.clearForTesting();
    }
}

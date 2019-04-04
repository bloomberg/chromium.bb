// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.preferences.website.ContentSettingValues;
import org.chromium.chrome.browser.webapps.WebappRegistry;

import java.util.ArrayList;
import java.util.List;

/**
 * Handles the preserving and surfacing the permissions of TWA client apps for their associated
 * websites. Communicates with the {@link org.chromium.chrome.browser.webapps.WebappRegistry} and
 * {@link InstalledWebappBridge}.
 *
 * Lifecycle: All methods are static.
 * Thread safety: Only call methods on a single thread.
 * Native: Does not require native.
 */
public class TrustedWebActivityPermissionManager {
    private static final String TAG = "TwaPermissionManager";

    static InstalledWebappBridge.Permission[] getNotificationPermissions() {
        // TODO: Hold reference to this.
        TrustedWebActivityPermissionStore store =
                WebappRegistry.getInstance().getTrustedWebActivityPermissionStore();

        List<InstalledWebappBridge.Permission> permissions = new ArrayList<>();
        for (String originAsString : store.getStoredOrigins()) {
            Origin origin = new Origin(originAsString);
            Boolean enabled = store.areNotificationsEnabled(origin);

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

    static void register(Origin origin, boolean notificationsEnabled) {
        TrustedWebActivityPermissionStore store =
                WebappRegistry.getInstance().getTrustedWebActivityPermissionStore();
        store.setNotificationState(origin, notificationsEnabled);
    }

    static void unregister(Origin origin) {
        TrustedWebActivityPermissionStore store =
                WebappRegistry.getInstance().getTrustedWebActivityPermissionStore();
        store.removeOrigin(origin);
    }

    @VisibleForTesting
    static void clearForTesting() {
        TrustedWebActivityPermissionStore store =
                WebappRegistry.getInstance().getTrustedWebActivityPermissionStore();
        store.clearForTesting();
    }
}

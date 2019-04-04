// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import android.content.Context;
import android.content.SharedPreferences;
import android.support.annotation.WorkerThread;

import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.browserservices.Origin;

import java.util.HashSet;
import java.util.Set;

/**
 * Stores cached data about the permissions (currently just the notification permission) of
 * installed Trusted Web Activity Client Apps. This is used to determine what permissions to give
 * to the associated websites. TWAs are indexed by their associated Origins.
 *
 * We did not use a similar technique to
 * {@link org.chromium.chrome.browser.webapps.WebappDataStorage}, because the data backing each
 * WebappDataStore is stored in its own Preferences file, so while
 * {@link org.chromium.chrome.browser.webapps.WebappRegistry} is eagerly loaded when Chrome starts
 * up, we don't want the first permission check to cause loading separate Preferences files for
 * each installed TWA.
 *
 *
 * Lifecycle: This class is designed to be owned by
 * {@link org.chromium.chrome.browser.webapps.WebappRegistry}, get it from there, don't create your
 * own instance.
 * Thread safety: Is thread-safe (only operates on underlying SharedPreferences).
 * Native: Does not require native.
 *
 * TODO(peconn): Unify this and WebappDataStorage?
 */
public class TrustedWebActivityPermissionStore {
    private static final String SHARED_PREFS_FILE = "twa_permission_registry";

    private static final String KEY_ALL_ORIGINS = "origins";
    private static final String KEY_NOTIFICATION_PERMISSION_PREFIX = "notification_permission.";

    private final SharedPreferences mPreferences;

    /**
     * Reads the underlying storage into memory, should be called initially on a background thread.
     */
    @WorkerThread
    public void initStorage() {
        // Read some value from the Preferences to ensure it's in memory.
        getStoredOrigins();
    }

    /** Creates a {@link TrustedWebActivityPermissionStore}. */
    public TrustedWebActivityPermissionStore() {
        mPreferences = ContextUtils.getApplicationContext().getSharedPreferences(
                SHARED_PREFS_FILE, Context.MODE_PRIVATE);
    }

    /**
     * Whether notifications for that origin should be enabled due to a TWA. {@code null} if
     * given origin is not linked to a TWA.
     */
    public Boolean areNotificationsEnabled(Origin origin) {
        String key = createNotificationPermissionKey(origin);
        if (!mPreferences.contains(key)) return null;
        return mPreferences.getBoolean(createNotificationPermissionKey(origin), false);
    }

    /** Gets all the origins of registered TWAs. */
    Set<String> getStoredOrigins() {
        // In case the pre-emptive disk read in initStorage hasn't occurred by the time we actually
        // need the value.
        try (StrictModeContext unused = StrictModeContext.allowDiskReads()){
            // The set returned by getStringSet cannot be modified.
            return new HashSet<>(mPreferences.getStringSet(KEY_ALL_ORIGINS, new HashSet<>()));
        }
    }

    /** Sets the notification state for the origin. */
    void setNotificationState(Origin origin, boolean enabled) {
        addOrigin(origin);

        SharedPreferences.Editor editor = mPreferences.edit();
        editor.putBoolean(createNotificationPermissionKey(origin), enabled);
        editor.apply();
    }

    /** Removes the origin from the store. */
    void removeOrigin(Origin origin) {
        Set<String> origins = getStoredOrigins();
        origins.remove(origin.toString());

        SharedPreferences.Editor editor = mPreferences.edit();
        editor.putStringSet(KEY_ALL_ORIGINS, origins);
        editor.remove(createNotificationPermissionKey(origin));
        editor.apply();
    }

    /** Clears the store, for testing. */
    @VisibleForTesting
    public void clearForTesting() {
        SharedPreferences.Editor editor = mPreferences.edit();
        editor.clear();
        editor.apply();
    }

    private void addOrigin(Origin origin) {
        Set<String> origins = getStoredOrigins();
        origins.add(origin.toString());

        SharedPreferences.Editor editor = mPreferences.edit();
        editor.putStringSet(KEY_ALL_ORIGINS, origins);
        editor.apply();
    }

    private String createNotificationPermissionKey(Origin origin) {
        return KEY_NOTIFICATION_PERMISSION_PREFIX + origin.toString();
    }
}

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import android.content.SharedPreferences;
import android.os.SystemClock;

import org.chromium.base.ContextUtils;
import org.chromium.base.TraceEvent;

/**
 * A utility class for applying operations on all loaded profiles.
 */
public class ProfileManagerUtils {
    // "ChromeMobileApplication" is a relic kept for backward compatibility.
    private static final String PREF_BOOT_TIMESTAMP =
            "com.google.android.apps.chrome.ChromeMobileApplication.BOOT_TIMESTAMP";
    private static final long BOOT_TIMESTAMP_MARGIN_MS = 1000;

    /**
     * Commits pending writes for all loaded profiles. The host activity should call this during its
     * onPause() handler to ensure all state is saved when the app is suspended.
     */
    public static void flushPersistentDataForAllProfiles() {
        try {
            TraceEvent.begin("ProfileManagerUtils.commitPendingWritesForAllProfiles");
            nativeFlushPersistentDataForAllProfiles();
        } finally {
            TraceEvent.end("ProfileManagerUtils.commitPendingWritesForAllProfiles");
        }
    }

    /**
     * Removes all session cookies (cookies with no expiration date). This should be called after
     * device reboots. This function incorrectly clears cookies when Daylight Savings Time changes
     * the clock. Without a way to get a monotonically increasing system clock, the boot timestamp
     * will be off by one hour. However, this should only happen at most once when the clock changes
     * since the updated timestamp is immediately saved.
     */
    public static void removeSessionCookiesForAllProfiles() {
        long lastKnownBootTimestamp =
                ContextUtils.getAppSharedPreferences().getLong(PREF_BOOT_TIMESTAMP, 0);
        long bootTimestamp = System.currentTimeMillis() - SystemClock.uptimeMillis();
        long difference = bootTimestamp - lastKnownBootTimestamp;

        // Allow some leeway to account for fractions of milliseconds.
        if (Math.abs(difference) > BOOT_TIMESTAMP_MARGIN_MS) {
            nativeRemoveSessionCookiesForAllProfiles();

            SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
            SharedPreferences.Editor editor = prefs.edit();
            editor.putLong(PREF_BOOT_TIMESTAMP, bootTimestamp);
            editor.apply();
        }
    }

    private static native void nativeFlushPersistentDataForAllProfiles();
    private static native void nativeRemoveSessionCookiesForAllProfiles();
}

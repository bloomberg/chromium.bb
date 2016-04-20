// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.preferences;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;

import org.chromium.blimp.R;

/**
 * Provides helper methods for storing and retrieving values in android shared preferences.
 */
public class PreferencesUtil {
    /**
     * Preference that stores the last used assigner URL.
     */
    private static final String PREF_LAST_USED_ASSIGNER = "last_known_assigner";
    private static final String DEFAULT_EMPTY_STRING = "";

    private static SharedPreferences getPreferences(Context context) {
        return PreferenceManager.getDefaultSharedPreferences(context.getApplicationContext());
    }

    /**
     * Finds the assigner to be used from user's last preference. If the app is being used for the
     * first time, the first entry from the assigner array would be used.
     * @return assigner to use.
     */
    public static String findAssignerUrl(Context context) {
        String lastAssigner = getPreferences(context).getString(PREF_LAST_USED_ASSIGNER, "");
        if (lastAssigner.isEmpty()) {
            String[] assignerUrls = context.getResources().getStringArray(R.array.assigner_urls);
            assert assignerUrls != null && assignerUrls.length > 0;
            lastAssigner = assignerUrls[0];
            writeString(context, PREF_LAST_USED_ASSIGNER, lastAssigner);
        }

        return lastAssigner;
    }

    /**
     * Reads the last used assigner from shared preference.
     * @param context The current Android context
     * @return The saved value of assigner preference
     */
    public static String getLastUsedAssigner(Context context) {
        return readString(context, PREF_LAST_USED_ASSIGNER);
    }

    /**
     * Sets the last used assigner.
     * @param context The current Android context
     * @param assigner The new value of assigner preference
     */
    public static void setLastUsedAssigner(Context context, String assigner) {
        writeString(context, PREF_LAST_USED_ASSIGNER, assigner);
    }

    /**
     * Reads a string value from shared preference.
     * @param context The current Android context
     * @param key The name of the preference to read
     * @return The current value of the preference or a default value
     */
    private static String readString(Context context, String key) {
        return getPreferences(context).getString(key, DEFAULT_EMPTY_STRING);
    }

    /**
     * Writes the given string into shared preference.
     * @param context The current Android context
     * @param key The name of the preference to modify
     * @param value The new value for the preference
     */
    private static void writeString(Context context, String key, String value) {
        SharedPreferences.Editor editor = getPreferences(context).edit();
        editor.putString(key, value);
        editor.apply();
    }
}

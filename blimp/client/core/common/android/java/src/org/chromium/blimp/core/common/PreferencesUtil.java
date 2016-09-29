// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core.common;

import org.chromium.base.ContextUtils;

/**
 * Blimp preferences utilities.
 */
public class PreferencesUtil {
    /**
     * Blimp switch preference key, also the key for this PreferenceFragment.
     */
    public static final String PREF_BLIMP_SWITCH = "blimp_switch";
    /**
     * Blimp assigner URL preference key.
     */
    public static final String PREF_ASSIGNER_URL = "blimp_assigner_url";

    /**
     * Default assigner URL, which is the production environment assigner.
     */
    public static final String DEFAULT_ASSIGNER_URL =
            "https://blimp-pa.googleapis.com/v1/assignment";

    /**
     * Reads the last used assigner from shared preference.
     * @return The saved value of assigner preference, or the default development assigner URL
     * if we didn't find the shared preference.
     */
    public static String getLastUsedAssigner() {
        return ContextUtils.getAppSharedPreferences().getString(
                PREF_ASSIGNER_URL, DEFAULT_ASSIGNER_URL);
    }

    /**
     * @return If Blimp switch preference in the setting page is turned on.
     */
    public static boolean isBlimpEnabled() {
        return ContextUtils.getAppSharedPreferences().getBoolean(PREF_BLIMP_SWITCH, false);
    }
}

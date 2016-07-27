// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core.settings;

import org.chromium.base.ContextUtils;

/**
 * Blimp preferences utilities.
 */
public class PreferencesUtil {
    private static final String DEFAULT_ASSIGNER_URL =
            "https://dev-blimp-pa.sandbox.googleapis.com/v1/assignment";

    /**
     * Reads the last used assigner from shared preference.
     * @return The saved value of assigner preference, or the default development assigner URL
     * if we didn't find the shared preference.
     */
    public static String getLastUsedAssigner() {
        return ContextUtils.getAppSharedPreferences().getString(
                AboutBlimpPreferences.PREF_ASSIGNER_URL, DEFAULT_ASSIGNER_URL);
    }
}

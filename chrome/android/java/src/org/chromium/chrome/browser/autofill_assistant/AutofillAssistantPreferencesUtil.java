// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.preferences.autofill_assistant.AutofillAssistantPreferences;

/** Autofill Assistant related preferences util class. */
public class AutofillAssistantPreferencesUtil {
    // Avoid instatiation by accident.
    private AutofillAssistantPreferencesUtil() {}

    /**
     * Peference keeping track of whether the initial screen should be skipped on startup.
     */
    private static final String AUTOFILL_ASSISTANT_SKIP_INIT_SCREEN =
            "AUTOFILL_ASSISTANT_SKIP_INIT_SCREEN";

    /** Checks whether the Autofill Assistant switch preference in settings is on. */
    public static boolean isAutofillAssistantSwitchOn() {
        return ContextUtils.getAppSharedPreferences().getBoolean(
                AutofillAssistantPreferences.PREF_AUTOFILL_ASSISTANT_SWITCH, true);
    }

    /** Gets whether skip initial screen preference. */
    public static boolean getSkipInitScreenPreference() {
        return ContextUtils.getAppSharedPreferences().getBoolean(
                AUTOFILL_ASSISTANT_SKIP_INIT_SCREEN, false);
    }

    /**
     * Sets preferences from the initial screen.
     *
     * @param accept Flag indicates whether this service is accepted.
     * @param dontShowAgain Flag indicates whether initial screen should be shown again next time.
     */
    public static void setInitialPreferences(boolean accept, boolean dontShowAgain) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(AutofillAssistantPreferences.PREF_AUTOFILL_ASSISTANT_SWITCH, accept)
                .apply();
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(AUTOFILL_ASSISTANT_SKIP_INIT_SCREEN, dontShowAgain)
                .apply();
    }
}

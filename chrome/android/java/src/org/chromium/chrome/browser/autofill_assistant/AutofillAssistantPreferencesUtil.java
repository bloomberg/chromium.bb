// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.preferences.autofill_assistant.AutofillAssistantPreferences;

/** Autofill Assistant related preferences util class. */
class AutofillAssistantPreferencesUtil {
    // Avoid instatiation by accident.
    private AutofillAssistantPreferencesUtil() {}

    /**
     * Peference keeping track of whether the initial screen should be skipped on startup.
     */
    private static final String AUTOFILL_ASSISTANT_SKIP_INIT_SCREEN =
            "AUTOFILL_ASSISTANT_SKIP_INIT_SCREEN";

    /** Checks whether the Autofill Assistant switch preference in settings is on. */
    static boolean isAutofillAssistantSwitchOn() {
        return ContextUtils.getAppSharedPreferences().getBoolean(
                AutofillAssistantPreferences.PREF_AUTOFILL_ASSISTANT_SWITCH, true);
    }

    /** Gets whether skip initial screen preference. */
    static boolean getSkipInitScreenPreference() {
        return ContextUtils.getAppSharedPreferences().getBoolean(
                AUTOFILL_ASSISTANT_SKIP_INIT_SCREEN, false);
    }

    /**
     * Returns true if the switch for AutofillAssistant is turned on or the init screen can
     * be shown. The later is important if the switched is turned off, but we can ask again
     * to enable AutofillAssistant.
     */
    static boolean canShowAutofillAssistant() {
        return isAutofillAssistantSwitchOn() || !getSkipInitScreenPreference();
    }

    /**
     * Sets preferences from the initial screen.
     *
     * @param accept Flag indicates whether this service is accepted.
     * @param dontShowAgain Flag indicates whether initial screen should be shown again next time.
     */
    static void setInitialPreferences(boolean accept) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(AutofillAssistantPreferences.PREF_AUTOFILL_ASSISTANT_SWITCH, accept)
                .apply();
        if (accept) {
            ContextUtils.getAppSharedPreferences()
                    .edit()
                    .putBoolean(AUTOFILL_ASSISTANT_SKIP_INIT_SCREEN, true)
                    .apply();
        }
    }
}

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.preferences.autofill_assistant.AutofillAssistantPreferences;

/**
 * Init screen controller, responsible for starting the native autofill assistant after init has
 * been confirmed.
 */
public class InitScreenController {
    /**
     * Name of the shared parameter entry keeping track of whether the
     * first screen should be skipped on startup.
     */
    private static final String AUTOFILL_ASSISTANT_SKIP_INIT_SCREEN =
            "AUTOFILL_ASSISTANT_SKIP_INIT_SCREEN";

    public static boolean skip() {
        return ContextUtils.getAppSharedPreferences().getBoolean(
                AUTOFILL_ASSISTANT_SKIP_INIT_SCREEN, false);
    }

    private final AutofillAssistantUiDelegate.Client mClient;

    /**
     * Constructs an Init screen.
     *
     * @param activity The parent activity.
     */
    public InitScreenController(AutofillAssistantUiDelegate.Client client) {
        mClient = client;
    }

    /**
     * The main callback method providing the result of the init, and the status
     * of the "don't show me again" checkbox.
     */
    public void onInitFinished(boolean initOk, boolean dontShowAgain) {
        if (initOk) {
            if (dontShowAgain) {
                ContextUtils.getAppSharedPreferences()
                        .edit()
                        .putBoolean(AUTOFILL_ASSISTANT_SKIP_INIT_SCREEN, true)
                        .apply();
            }
            mClient.onInitOk();
            return;
        } else {
            mClient.onInitRejected();
        }
        if (dontShowAgain) {
            ContextUtils.getAppSharedPreferences()
                    .edit()
                    .putBoolean(AutofillAssistantPreferences.PREF_AUTOFILL_ASSISTANT_SWITCH, false)
                    .apply();
        }
    }
}

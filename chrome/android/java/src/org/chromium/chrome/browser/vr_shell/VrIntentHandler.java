// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.content.Intent;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.IntentHandler;

// TODO(bsheedy): Move more VR intent related code into here even if it doesn't need
// to be overridden for testing
/**
 * Handles VR intent checking for VrShellDelegate.
 */
public class VrIntentHandler {
    private static VrIntentHandler sInstance;
    private static final String DAYDREAM_HOME_PACKAGE = "com.google.android.vr.home";

    /**
     * Determines whether the given intent is a VR intent from Daydream Home.
     * @param intent The intent to check
     * @return Whether the intent is a VR intent and originated from Daydream Home
     */
    public boolean isTrustedDaydreamIntent(Intent intent) {
        return VrShellDelegate.isVrIntent(intent)
                && IntentHandler.isIntentFromTrustedApp(intent, DAYDREAM_HOME_PACKAGE);
    }

    /**
     * Gets the static VrIntentHandler instance.
     * @return The VrIntentHandler instance
     */
    public static VrIntentHandler getInstance() {
        if (sInstance == null) {
            sInstance = new VrIntentHandler();
        }
        return sInstance;
    }

    @VisibleForTesting
    public static void setInstanceForTesting(VrIntentHandler handler) {
        sInstance = handler;
    }
}

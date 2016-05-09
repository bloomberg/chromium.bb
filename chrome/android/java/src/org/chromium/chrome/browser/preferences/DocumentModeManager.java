// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.Context;
import android.content.SharedPreferences;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;

/**
 * Tracks opt out status for document mode
 */
public class DocumentModeManager {

    public static final String OPT_OUT_STATE = "opt_out_state";
    public static final int OPT_OUT_STATE_UNSET = -1;
    public static final int OPT_IN_TO_DOCUMENT_MODE = 0;
    public static final int OPT_OUT_PROMO_DISMISSED = 1;
    public static final int OPTED_OUT_OF_DOCUMENT_MODE = 2;
    public static final String OPT_OUT_SHOWN_COUNT = "opt_out_shown_count";
    public static final String OPT_OUT_CLEAN_UP_PENDING = "opt_out_clean_up_pending";

    private static DocumentModeManager sManager;

    private final SharedPreferences mSharedPreferences;

    private DocumentModeManager(Context context) {
        mSharedPreferences = ContextUtils.getAppSharedPreferences();
    }

    /**
     * Get the static instance of DocumentModeManager if it exists, else create it.
     * @param context The current Android context
     * @return the DocumentModeManager singleton
     */
    public static DocumentModeManager getInstance(Context context) {
        ThreadUtils.assertOnUiThread();
        if (sManager == null) {
            sManager = new DocumentModeManager(context);
        }
        return sManager;
    }

    /**
     * @return Whether the user set a preference to not use the document mode.
     */
    public boolean isOptedOutOfDocumentMode() {
        return getOptOutState() == OPTED_OUT_OF_DOCUMENT_MODE;
    }

    /**
     * Sets the opt out preference.
     * @param state One of OPTED_OUT_OF_DOCUMENT_MODE or OPT_OUT_PROMO_DISMISSED.
     */
    public void setOptedOutState(int state) {
        SharedPreferences.Editor sharedPreferencesEditor = mSharedPreferences.edit();
        sharedPreferencesEditor.putInt(OPT_OUT_STATE, state);
        sharedPreferencesEditor.apply();
    }

    /**
     * @return Whether we need to clean up old document activity tasks from Recents.
     */
    public boolean isOptOutCleanUpPending() {
        return mSharedPreferences.getBoolean(OPT_OUT_CLEAN_UP_PENDING, false);
    }

    /**
     * Mark that we need to clean up old documents from Recents or reset it after the task
     * is done.
     * @param pending Whether we need to clean up.
     */
    public void setOptOutCleanUpPending(boolean pending) {
        SharedPreferences.Editor sharedPreferencesEditor = mSharedPreferences.edit();
        sharedPreferencesEditor.putBoolean(OPT_OUT_CLEAN_UP_PENDING, pending);
        sharedPreferencesEditor.apply();
    }

    private int getOptOutState() {
        int optOutState = mSharedPreferences.getInt(OPT_OUT_STATE, OPT_OUT_STATE_UNSET);
        if (optOutState == OPT_OUT_STATE_UNSET) {
            boolean hasMigrated = mSharedPreferences.getBoolean(
                    ChromePreferenceManager.MIGRATION_ON_UPGRADE_ATTEMPTED, false);
            if (!hasMigrated) {
                optOutState = OPTED_OUT_OF_DOCUMENT_MODE;
            } else {
                optOutState = OPT_IN_TO_DOCUMENT_MODE;
            }
            setOptedOutState(optOutState);
        }
        return optOutState;
    }

    @VisibleForTesting
    public int getOptOutStateForTesting() {
        return mSharedPreferences.getInt(OPT_OUT_STATE, OPT_OUT_STATE_UNSET);
    }
}

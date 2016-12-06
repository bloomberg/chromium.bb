// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.app.Activity;
import android.content.Intent;
import android.text.TextUtils;

import org.chromium.base.CommandLine;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.components.variations.VariationsAssociatedData;

/**
 * Utility methods for the browsing history manager.
 */
public class HistoryManagerUtils {
    private static final String FIELD_TRIAL_NAME = "AndroidHistoryManager";
    private static final String ENABLE_HISTORY_SWTICH = "enable_android_history_manager";
    private static final Object NATIVE_HISTORY_ENABLED_LOCK = new Object();
    private static Boolean sNativeHistoryEnabled;

    /**
    * @return Whether the Android-specific browsing history manager is enabled.
    */
    public static boolean isAndroidHistoryManagerEnabled() {
        synchronized (NATIVE_HISTORY_ENABLED_LOCK) {
            if (sNativeHistoryEnabled == null) {
                if (CommandLine.getInstance().hasSwitch(ENABLE_HISTORY_SWTICH)) {
                    sNativeHistoryEnabled = true;
                } else {
                    sNativeHistoryEnabled = TextUtils.equals("true",
                            VariationsAssociatedData.getVariationParamValue(FIELD_TRIAL_NAME,
                                    ENABLE_HISTORY_SWTICH));
                }
            }
        }

        return sNativeHistoryEnabled;
    }

    /**
    * @return Whether the Android-specific browsing history UI is was shown.
    */
    public static boolean showHistoryManager(Activity activity) {
        if (!isAndroidHistoryManagerEnabled()) return false;

        // TODO(twellington): Add support for tablets
        Intent intent = new Intent();
        intent.setClass(activity.getApplicationContext(), HistoryActivity.class);
        intent.putExtra(IntentHandler.EXTRA_PARENT_COMPONENT, activity.getComponentName());
        activity.startActivity(intent);

        return true;
    }
}
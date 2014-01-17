// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;

import org.chromium.base.CalledByNative;
import org.chromium.base.ThreadUtils;

/**
 * Stub entry points and implementation interface for the account management screen.
 */
public class AccountManagementScreenHelper {
    private static AccountManagementScreenManager sManager;

    /**
     * The screen manager interface.
     */
    public interface AccountManagementScreenManager {
        /**
         * Opens the account management UI screen.
         * @param applicationContext The application context.
         */
        void openAccountManagementScreen(Context applicationContext);
    }

    /**
     * Sets the screen manager.
     * @param manager An implementation of AccountManagementScreenManager interface.
     */
    public static void setManager(AccountManagementScreenManager manager) {
        ThreadUtils.assertOnUiThread();
        sManager = manager;
    }

    @CalledByNative
    private static void openAccountManagementScreen(Context applicationContext) {
        ThreadUtils.assertOnUiThread();
        if (sManager == null) return;

        sManager.openAccountManagementScreen(applicationContext);
    }
}

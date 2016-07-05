// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.app.Activity;
import android.text.TextUtils;

import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.sync.signin.ChromeSigninController;

/**
* Helper functions for promoting sign in.
*/
public class SigninPromoUtil {
    private SigninPromoUtil() {}

    /**
     * Launches the signin promo if it needs to be displayed.
     * @param activity The parent activity.
     * @return Whether the signin promo is shown.
     */
    public static boolean launchSigninPromoIfNeeded(final Activity activity) {
        // The promo is displayed if Chrome is launched directly (i.e., not with the intent to
        // navigate to and view a URL on startup), the instance is part of the field trial,
        // and the promo has been marked to display.
        ChromePreferenceManager preferenceManager = ChromePreferenceManager.getInstance(activity);
        if (MultiWindowUtils.getInstance().isLegacyMultiWindow(activity)) return false;
        if (!preferenceManager.getShowSigninPromo()) return false;
        preferenceManager.setShowSigninPromo(false);

        String lastSyncName = PrefServiceBridge.getInstance().getSyncLastAccountName();
        if (ChromeSigninController.get(activity).isSignedIn() || !TextUtils.isEmpty(lastSyncName)) {
            return false;
        }

        AccountSigninActivity.startAccountSigninActivity(activity, SigninAccessPoint.SIGNIN_PROMO);
        preferenceManager.setSigninPromoShown();
        return true;
    }
}

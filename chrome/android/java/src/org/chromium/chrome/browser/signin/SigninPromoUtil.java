// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.app.Activity;
import android.text.TextUtils;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.ui.base.WindowAndroid;

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
        ChromePreferenceManager preferenceManager = ChromePreferenceManager.getInstance();
        int lastPromoMajorVersion = preferenceManager.getSigninPromoLastShownVersion();
        int currentMajorVersion = ChromeVersionInfo.getProductMajorVersion();
        if (lastPromoMajorVersion == 0) {
            preferenceManager.setSigninPromoLastShownVersion(currentMajorVersion);
            return false;
        }

        // Don't show if user is signed in or there are no Google accounts on the device.
        if (ChromeSigninController.get().isSignedIn()) return false;
        if (AccountManagerFacade.get().tryGetGoogleAccountNames().size() == 0) return false;

        // Don't show if user has manually signed out.
        String lastSyncAccountName = PrefServiceBridge.getInstance().getSyncLastAccountName();
        if (TextUtils.isEmpty(lastSyncAccountName)) return false;

        // Promo can be shown at most once every 2 Chrome major versions.
        if (currentMajorVersion < lastPromoMajorVersion + 2) return false;

        AccountSigninActivity.startIfAllowed(activity, SigninAccessPoint.SIGNIN_PROMO);
        preferenceManager.setSigninPromoLastShownVersion(currentMajorVersion);
        return true;
    }

    /**
     * A convenience method to create an AccountSigninActivity, passing the access point as an
     * intent extra.
     * @param window WindowAndroid from which to get the Activity/Context.
     * @param accessPoint for metrics purposes.
     */
    @CalledByNative
    private static void openAccountSigninActivityForPromo(WindowAndroid window, int accessPoint) {
        Activity activity = window.getActivity().get();
        if (activity != null) {
            AccountSigninActivity.startIfAllowed(activity, accessPoint);
        }
    }
}

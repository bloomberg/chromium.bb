// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.chromium.base.ContextUtils;

/** Place to define and control payment preferences. */
public class PaymentPreferencesUtil {
    // Avoid instantiation by accident.
    private PaymentPreferencesUtil() {}

    /** Preference to indicate whether payment request has been completed successfully once.*/
    private static final String PAYMENT_COMPLETE_ONCE = "payment_complete_once";

    /** Prefix of the preferences to persist Android payment apps' status. */
    public static final String PAYMENT_ANDROID_APP_ENABLED_ = "payment_android_app_enabled_";

    /**
     * Checks whehter the payment request has been successfully completed once.
     *
     * @return True If payment request has been successfully completed once.
     */
    public static boolean isPaymentCompleteOnce() {
        return ContextUtils.getAppSharedPreferences().getBoolean(PAYMENT_COMPLETE_ONCE, false);
    }

    /** Sets the payment request has been successfully completed once. */
    public static void setPaymentCompleteOnce() {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(PAYMENT_COMPLETE_ONCE, true)
                .apply();
    }

    /**
     * Checks whether the Android payment app is enabled by user from the settings. The default
     * status is enabled.
     *
     * @param packageName The package name of the Android payment app.
     * @return True If the Android payment app is enabled by user.
     */
    public static boolean isAndroidPaymentAppEnabled(String packageName) {
        return ContextUtils.getAppSharedPreferences().getBoolean(
                getAndroidPaymentAppEnabledPreferenceKey(packageName), true);
    }

    /**
     * Gets preference key for status of the Android payment app.
     *
     * @param packageName The packageName of the Android payment app.
     * @return The preference key.
     */
    public static String getAndroidPaymentAppEnabledPreferenceKey(String packageName) {
        return PAYMENT_ANDROID_APP_ENABLED_ + packageName;
    }
}

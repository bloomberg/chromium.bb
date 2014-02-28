// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.CalledByNative;
import org.chromium.chrome.browser.Tab;

/**
 * Allows embedders to authenticate the usage of passwords.
 */
public class PasswordAuthenticationManager {

    /**
     * The delegate that allows embedders to control the authentication of passwords.
     */
    public interface PasswordAuthenticationDelegate {
        /**
         * @return Whether password authentication is enabled.
         */
        boolean isPasswordAuthenticationEnabled();

        /**
         * Requests password authentication be presented for the given tab.
         * @param tab The tab containing the protected password.
         * @param callback The callback to be triggered on authentication result.
         */
        void requestAuthentication(Tab tab, PasswordAuthenticationCallback callback);

        /**
         * @return The message to be displayed in the save password infobar that will allow
         *         the user to opt-in to additional password authentication.
         */
        String getPasswordProtectionString();
    }

    /**
     * The callback to be triggered on success or failure of the password authentication.
     */
    public static class PasswordAuthenticationCallback {
        private long mNativePtr;

        @CalledByNative("PasswordAuthenticationCallback")
        private static PasswordAuthenticationCallback create(long nativePtr) {
            return new PasswordAuthenticationCallback(nativePtr);
        }

        private PasswordAuthenticationCallback(long nativePtr) {
            mNativePtr = nativePtr;
        }

        /**
         * Called upon authentication results to allow usage of the password or not.
         * @param authenticated Whether the authentication was successful.
         */
        public final void onResult(boolean authenticated) {
            if (mNativePtr == 0) {
                assert false : "Can not call onResult more than once per callback.";
                return;
            }
            nativeOnResult(mNativePtr, authenticated);
            mNativePtr = 0;
        }
    }

    private static class DefaultPasswordAuthenticationDelegate
            implements PasswordAuthenticationDelegate {
        @Override
        public boolean isPasswordAuthenticationEnabled() {
            return false;
        }

        @Override
        public void requestAuthentication(Tab tab, PasswordAuthenticationCallback callback) {
            callback.onResult(true);
        }

        @Override
        public String getPasswordProtectionString() {
            return "";
        }
    }

    private static PasswordAuthenticationDelegate sDelegate;

    private PasswordAuthenticationManager() {}

    private static PasswordAuthenticationDelegate getDelegate() {
        if (sDelegate == null) {
            sDelegate = new DefaultPasswordAuthenticationDelegate();
        }
        return sDelegate;
    }

    /**
     * Sets the password authentication delegate to be used.
     */
    public static void setDelegate(PasswordAuthenticationDelegate delegate) {
        sDelegate = delegate;
    }

    /**
     * @return Whether password authentication is enabled.
     */
    public static boolean isPasswordAuthenticationEnabled() {
        return getDelegate().isPasswordAuthenticationEnabled();
    }

    /**
     * Requests password authentication be presented for the given tab.
     * @param tab The tab containing the protected password.
     * @param callback The callback to be triggered on authentication result.
     */
    @CalledByNative
    public static void requestAuthentication(
            Tab tab, PasswordAuthenticationCallback callback) {
        getDelegate().requestAuthentication(tab, callback);
    }

    /**
     * @return The message to be displayed in the save password infobar that will allow the user
     *         to opt-in to additional password authentication.
     */
    public static String getPasswordProtectionString() {
        return getDelegate().getPasswordProtectionString();
    }

    private static native void nativeOnResult(long callbackPtr, boolean authenticated);
}

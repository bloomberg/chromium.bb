// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * A utility class for checking if the device is currently connected to the Internet.
 */
@JNINamespace("chrome::android")
public final class ConnectivityChecker {
    private static final String DEFAULT_NO_CONTENT_URL = "http://clients4.google.com/generate_204";

    /**
     * A callback for whether the device is currently connected to the Internet.
     */
    public interface ConnectivityCheckerCallback {
        /**
         * Called when the result of the connectivity check is ready.
         */
        void onResult(boolean connected);
    }

    /**
     * Starts an asynchronous request for checking whether the device is currently connected to the
     * Internet. The result passed to the callback denotes whether the attempt to connect to the
     * server was successful.
     *
     * If the profile or URL is invalid, the callback will be called with false.
     * The server reply for the URL must respond with HTTP 204 without any redirects for the
     * connectivity check to be successful.
     *
     * This method takes ownership of the callback object until the callback has happened.
     * This method must be called on the main thread.
     * @param profile the context to do the check in.
     * @param timeoutMs number of milliseconds to wait before giving up waiting for a connection.
     * @param callback the callback which will get the result.
     */
    public static void checkConnectivity(
            Profile profile, long timeoutMs, ConnectivityCheckerCallback callback) {
        checkConnectivity(profile, DEFAULT_NO_CONTENT_URL, timeoutMs, callback);
    }

    @VisibleForTesting
    static void checkConnectivity(
            Profile profile, String url, long timeoutMs, ConnectivityCheckerCallback callback) {
        ThreadUtils.assertOnUiThread();
        nativeCheckConnectivity(profile, url, timeoutMs, callback);
    }

    @CalledByNative
    private static void executeCallback(Object callback, boolean connected) {
        ((ConnectivityCheckerCallback) callback).onResult(connected);
    }

    private ConnectivityChecker() {}

    private static native void nativeCheckConnectivity(
            Profile profile, String url, long timeoutMs, ConnectivityCheckerCallback callback);
}

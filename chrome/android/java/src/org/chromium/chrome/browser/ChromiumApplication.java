// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.base.CalledByNative;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.content.app.ContentApplication;

/**
 * Basic application functionality that should be shared among all browser applications that use
 * chrome layer.
 */
public abstract class ChromiumApplication extends ContentApplication {

    /**
     * Returns the class name of the Settings activity.
     */
    public String getSettingsActivityName() {
        return Preferences.class.getName();
    }

    /**
     * Opens a protected content settings page, if available.
     */
    @CalledByNative
    protected void openProtectedContentSettings() {}

    @CalledByNative
    protected void showAutofillSettings() {}

    @CalledByNative
    protected void showPasswordSettings() {}

    /**
     * Opens the UI to clear browsing data.
     * @param tab The tab that triggered the request.
     */
    @CalledByNative
    protected void openClearBrowsingData(Tab tab) {}

    /**
     * @return Whether parental controls are enabled.  Returning true will disable
     *         incognito mode.
     */
    @CalledByNative
    protected abstract boolean areParentalControlsEnabled();

    // TODO(yfriedman): This is too widely available. Plumb this through ChromeNetworkDelegate
    // instead.
    protected abstract PKCS11AuthenticationManager getPKCS11AuthenticationManager();

    /**
     * @return The user agent string of Chrome.
     */
    public static String getBrowserUserAgent() {
        return nativeGetBrowserUserAgent();
    }

    private static native String nativeGetBrowserUserAgent();
}

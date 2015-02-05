// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.os.Build;

import org.chromium.base.CalledByNative;
import org.chromium.chrome.browser.preferences.LocationSettings;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.ProtectedContentPreferences;
import org.chromium.chrome.browser.preferences.autofill.AutofillPreferences;
import org.chromium.chrome.browser.preferences.password.ManageSavedPasswordsPreferences;
import org.chromium.content.app.ContentApplication;

/**
 * Basic application functionality that should be shared among all browser applications that use
 * chrome layer.
 */
public abstract class ChromiumApplication extends ContentApplication {

    /**
     * Returns the class name of the Settings activity.
     */
    public abstract String getSettingsActivityName();

    /**
     * Opens a protected content settings page, if available.
     */
    @CalledByNative
    protected void openProtectedContentSettings() {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT;
        PreferencesLauncher.launchSettingsPage(this,
                ProtectedContentPreferences.class.getName());
    }

    @CalledByNative
    protected void showAutofillSettings() {
        PreferencesLauncher.launchSettingsPage(this,
                AutofillPreferences.class.getName());
    }

    @CalledByNative
    protected void showPasswordSettings() {
        PreferencesLauncher.launchSettingsPage(this,
                ManageSavedPasswordsPreferences.class.getName());
    }

    /**
     * Returns an instance of LocationSettings to be installed as a singleton.
     */
    public LocationSettings createLocationSettings() {
        return new LocationSettings(this);
    }

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

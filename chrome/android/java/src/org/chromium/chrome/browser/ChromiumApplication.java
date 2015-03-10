// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;

import org.chromium.base.CalledByNative;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryProcessType;;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.preferences.LocationSettings;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.ProtectedContentPreferences;
import org.chromium.chrome.browser.preferences.autofill.AutofillPreferences;
import org.chromium.chrome.browser.preferences.password.ManageSavedPasswordsPreferences;
import org.chromium.chrome.browser.preferences.privacy.PrivacyPreferences;
import org.chromium.chrome.browser.preferences.website.SingleWebsitePreferences;
import org.chromium.content.app.ContentApplication;
import org.chromium.content.browser.BrowserStartupController;

/**
 * Basic application functionality that should be shared among all browser applications that use
 * chrome layer.
 */
public abstract class ChromiumApplication extends ContentApplication {

    private static final String TAG = "ChromiumApplication";

    /**
     * Returns whether the Activity is being shown in multi-window mode.
     */
    public boolean isMultiWindow(Activity activity) {
        return false;
    }

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
     * Opens the single origin settings page for the given URL.
     *
     * @param url The URL to show the single origin settings for. This is a complete url
     *            including scheme, domain, port, path, etc.
     */
    protected void showSingleOriginSettings(String url) {
        Bundle fragmentArgs = SingleWebsitePreferences.createFragmentArgsForSite(url);
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(
                this, SingleWebsitePreferences.class.getName());
        intent.putExtra(Preferences.EXTRA_SHOW_FRAGMENT_ARGUMENTS, fragmentArgs);
        startActivity(intent);
    }

    /**
     * For extending classes to carry out tasks that initialize the browser process.
     * Should be called almost immediately after the native library has loaded to initialize things
     * that really, really have to be set up early.  Avoid putting any long tasks here.
     */
    public void initializeProcess() {
        DataReductionProxySettings.initialize(getApplicationContext());
    }

    /**
     * Start the browser process asynchronously. This will set up a queue of UI
     * thread tasks to initialize the browser process.
     *
     * Note that this can only be called on the UI thread.
     *
     * @param callback the callback to be called when browser startup is complete.
     * @throws ProcessInitException
     */
    public void startChromeBrowserProcessesAsync(BrowserStartupController.StartupCallback callback)
            throws ProcessInitException {
        assert ThreadUtils.runningOnUiThread() : "Tried to start the browser on the wrong thread";
        Context applicationContext = getApplicationContext();
        BrowserStartupController.get(applicationContext, LibraryProcessType.PROCESS_BROWSER)
                .startBrowserProcessesAsync(callback);
    }

    /**
     * Returns an instance of LocationSettings to be installed as a singleton.
     */
    public LocationSettings createLocationSettings() {
        // Using an anonymous subclass as the constructor is protected.
        // This is done to deter instantiation of LocationSettings elsewhere without using the
        // getInstance() helper method.
        return new LocationSettings(this){};
    }

    /**
     * Opens the UI to clear browsing data.
     * @param tab The tab that triggered the request.
     */
    @CalledByNative
    protected void openClearBrowsingData(Tab tab) {
        Activity activity = tab.getWindowAndroid().getActivity().get();
        if (activity == null) {
            Log.e(TAG,
                    "Attempting to open clear browsing data for a tab without a valid activity");
            return;
        }
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(activity,
                PrivacyPreferences.class.getName());
        Bundle arguments = new Bundle();
        arguments.putBoolean(PrivacyPreferences.SHOW_CLEAR_BROWSING_DATA_EXTRA, true);
        intent.putExtra(Preferences.EXTRA_SHOW_FRAGMENT_ARGUMENTS, arguments);
        activity.startActivity(intent);
    }

    /**
     * @return Whether parental controls are enabled.  Returning true will disable
     *         incognito mode.
     */
    @CalledByNative
    protected boolean areParentalControlsEnabled() {
        return PartnerBrowserCustomizations.isIncognitoDisabled();
    }

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

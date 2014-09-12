// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.shell;

import android.content.Intent;
import android.util.Log;

import org.chromium.base.CommandLine;
import org.chromium.base.PathUtils;
import org.chromium.base.ResourceExtractor;
import org.chromium.chrome.browser.ChromiumApplication;
import org.chromium.chrome.browser.PKCS11AuthenticationManager;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.UmaUtils;
import org.chromium.chrome.browser.invalidation.UniqueIdInvalidationClientNameGenerator;

import java.util.ArrayList;

/**
 * A basic test shell {@link android.app.Application}.  Handles setting up the native library and
 * loading the right resources.
 */
public class ChromeShellApplication extends ChromiumApplication {
    private static final String TAG = "ChromeShellApplication";

    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "chromeshell";
    /**
     * icudtl.dat provides ICU (i18n library) with all the necessary data
     * for its operation. We used to link the data statically to our binary,
     * but don't do that any more and need to install along with pak files.
     * See src/third_party/icu/README.chromium.
     */
    private static final String[] CHROME_MANDATORY_PAKS = {
        "en-US.pak",
        "resources.pak",
        "chrome_100_percent.pak",
        "icudtl.dat",
    };
    private static final String COMMAND_LINE_FILE = "/data/local/tmp/chrome-shell-command-line";

    ArrayList<ChromeShellApplicationObserver> mObservers;

    @Override
    public void onCreate() {
        // We want to do this at the earliest possible point in startup.
        UmaUtils.recordMainEntryPointTime();
        super.onCreate();

        ResourceExtractor.setMandatoryPaksToExtract(CHROME_MANDATORY_PAKS);
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);

        mObservers = new ArrayList<ChromeShellApplicationObserver>();

        // Initialize the invalidations ID, just like we would in the downstream code.
        UniqueIdInvalidationClientNameGenerator.doInitializeAndInstallGenerator(this);
    }

    @Override
    public void sendBroadcast(Intent intent) {
        boolean shouldFire = true;
        for (ChromeShellApplicationObserver observer : mObservers) {
            shouldFire &= observer.onSendBroadcast(intent);
        }

        if (shouldFire) super.sendBroadcast(intent);
    }

    public void addObserver(ChromeShellApplicationObserver observer) {
        mObservers.add(observer);
    }

    public void removeObserver(ChromeShellApplicationObserver observer) {
        mObservers.remove(observer);
    }

    public static void initCommandLine() {
        if (!CommandLine.isInitialized()) {
            CommandLine.initFromFile(COMMAND_LINE_FILE);
        }
    }

    @Override
    protected void openProtectedContentSettings() {
    }

    @Override
    protected void showSyncSettings() {
    }

    @Override
    protected void showAutofillSettings() {
    }

    @Override
    protected void showTermsOfServiceDialog() {
    }

    @Override
    protected void openClearBrowsingData(Tab tab) {
        Log.e(TAG, "Clear browsing data not currently supported in Chrome Shell");
    }

    @Override
    protected boolean areParentalControlsEnabled() {
        return false;
    }

    @Override
    protected PKCS11AuthenticationManager getPKCS11AuthenticationManager() {
        return new ChromeShellPKCS11AuthenticationManager();
    }
}

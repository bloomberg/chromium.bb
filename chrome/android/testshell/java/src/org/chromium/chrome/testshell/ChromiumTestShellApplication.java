// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.testshell;

import android.content.Intent;

import org.chromium.base.CommandLine;
import org.chromium.base.PathUtils;
import org.chromium.chrome.browser.ChromiumApplication;
import org.chromium.chrome.browser.PKCS11AuthenticationManager;
import org.chromium.chrome.browser.UmaUtils;
import org.chromium.chrome.browser.invalidation.UniqueIdInvalidationClientNameGenerator;
import org.chromium.content.browser.ResourceExtractor;

import java.util.ArrayList;

/**
 * A basic test shell {@link android.app.Application}.  Handles setting up the native library and
 * loading the right resources.
 */
public class ChromiumTestShellApplication extends ChromiumApplication {
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "chromiumtestshell";
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
    private static final String COMMAND_LINE_FILE =
            "/data/local/tmp/chromium-testshell-command-line";

    ArrayList<ChromiumTestShellApplicationObserver> mObservers;

    @Override
    public void onCreate() {
        // We want to do this at the earliest possible point in startup.
        UmaUtils.recordMainEntryPointTime();
        super.onCreate();

        ResourceExtractor.setMandatoryPaksToExtract(CHROME_MANDATORY_PAKS);
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);

        mObservers = new ArrayList<ChromiumTestShellApplicationObserver>();

        // Initialize the invalidations ID, just like we would in the downstream code.
        UniqueIdInvalidationClientNameGenerator.doInitializeAndInstallGenerator(this);
    }

    @Override
    public void sendBroadcast(Intent intent) {
        boolean shouldFire = true;
        for (ChromiumTestShellApplicationObserver observer : mObservers) {
            shouldFire &= observer.onSendBroadcast(intent);
        }

        if (shouldFire) {
            super.sendBroadcast(intent);
        }
    }

    public void addObserver(ChromiumTestShellApplicationObserver observer) {
        mObservers.add(observer);
    }

    public void removeObserver(ChromiumTestShellApplicationObserver observer) {
        mObservers.remove(observer);
    }

    public static void initCommandLine() {
        if (!CommandLine.isInitialized())
            CommandLine.initFromFile(COMMAND_LINE_FILE);
    }

    @Override
    protected void openProtectedContentSettings() {
    }

    @Override
    protected void showSyncSettings() {
    }

    @Override
    protected void showTermsOfServiceDialog() {
    }

    @Override
    protected boolean areParentalControlsEnabled() {
        return false;
    }

    @Override
    protected PKCS11AuthenticationManager getPKCS11AuthenticationManager() {
        return new TestShellPKCS11AuthenticationManager();
    }
}

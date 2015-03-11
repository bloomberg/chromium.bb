// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.shell;

import android.content.Intent;

import org.chromium.base.PathUtils;
import org.chromium.base.ResourceExtractor;
import org.chromium.chrome.browser.ChromiumApplication;
import org.chromium.chrome.browser.PKCS11AuthenticationManager;
import org.chromium.chrome.browser.identity.UniqueIdentificationGeneratorFactory;
import org.chromium.chrome.browser.identity.UuidBasedUniqueIdentificationGenerator;
import org.chromium.chrome.browser.invalidation.UniqueIdInvalidationClientNameGenerator;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.sync.SyncController;
import org.chromium.chrome.shell.preferences.ChromeShellPreferences;

import java.util.ArrayList;

/**
 * A basic test shell {@link android.app.Application}.  Handles setting up the native library and
 * loading the right resources.
 */
public class ChromeShellApplication extends ChromiumApplication {

    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "chromeshell";
    /**
     * icudtl.dat provides ICU (i18n library) with all the necessary data
     * for its operation. We used to link the data statically to our binary,
     * but don't do that any more and need to install along with pak files.
     * See src/third_party/icu/README.chromium.
     *
     *  V8's initial snapshot used to be statically linked to the binary, but
     *  now it's loaded from external files. Therefore we need to install such
     *  snapshots (natives_blob.bin and snapshot.bin) along with pak files.
     */
    private static final String[] CHROME_MANDATORY_PAKS = {
        "en-US.pak",
        "resources.pak",
        "chrome_100_percent.pak",
        "icudtl.dat",
        "natives_blob.bin",
        "snapshot_blob.bin"
    };
    private static final String COMMAND_LINE_FILE = "/data/local/tmp/chrome-shell-command-line";

    private static final String SESSIONS_UUID_PREF_KEY = "chromium.sync.sessions.id";

    ArrayList<ChromeShellApplicationObserver> mObservers;

    @Override
    public void onCreate() {
        // We want to do this at the earliest possible point in startup.
        UmaUtils.recordMainEntryPointTime();
        super.onCreate();

        // Assume that application start always leads to meaningful UMA startup metrics. This is not
        // the case for the official Chrome on Android.
        UmaUtils.setRunningApplicationStart(true);

        mObservers = new ArrayList<ChromeShellApplicationObserver>();

        // Initialize the invalidations ID, just like we would in the downstream code.
        UniqueIdInvalidationClientNameGenerator.doInitializeAndInstallGenerator(this);

        // Set up the identification generator for sync. The ID is actually generated
        // in the SyncController constructor.
        UniqueIdentificationGeneratorFactory.registerGenerator(SyncController.GENERATOR_ID,
                new UuidBasedUniqueIdentificationGenerator(this, SESSIONS_UUID_PREF_KEY), false);
    }

    @Override
    protected void initializeLibraryDependencies() {
        ResourceExtractor.setMandatoryPaksToExtract(CHROME_MANDATORY_PAKS);
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
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

    @Override
    public String getSettingsActivityName() {
        return ChromeShellPreferences.class.getName();
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

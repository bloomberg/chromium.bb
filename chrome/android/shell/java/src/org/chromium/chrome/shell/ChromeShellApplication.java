// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.shell;

import org.chromium.base.CommandLine;
import org.chromium.base.PathUtils;
import org.chromium.base.ResourceExtractor;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.identity.UniqueIdentificationGeneratorFactory;
import org.chromium.chrome.browser.identity.UuidBasedUniqueIdentificationGenerator;
import org.chromium.chrome.browser.invalidation.UniqueIdInvalidationClientNameGenerator;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.smartcard.EmptyPKCS11AuthenticationManager;
import org.chromium.chrome.browser.smartcard.PKCS11AuthenticationManager;
import org.chromium.chrome.browser.sync.SyncController;
import org.chromium.chrome.shell.preferences.ChromeShellPreferences;
import org.chromium.ui.base.ResourceBundle;

/**
 * A basic test shell {@link android.app.Application}.  Handles setting up the native library and
 * loading the right resources.
 */
public class ChromeShellApplication extends ChromeApplication {

    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "chromeshell";
    private static final String COMMAND_LINE_FILE = "/data/local/tmp/chrome-shell-command-line";

    private static final String SESSIONS_UUID_PREF_KEY = "chromium.sync.sessions.id";

    @Override
    public void onCreate() {
        // We want to do this at the earliest possible point in startup.
        UmaUtils.recordMainEntryPointTime();
        super.onCreate();

        // Assume that application start always leads to meaningful UMA startup metrics. This is not
        // the case for the official Chrome on Android.
        UmaUtils.setRunningApplicationStart(true);

        // Initialize the invalidations ID, just like we would in the downstream code.
        UniqueIdInvalidationClientNameGenerator.doInitializeAndInstallGenerator(this);

        // Set up the identification generator for sync. The ID is actually generated
        // in the SyncController constructor.
        UniqueIdentificationGeneratorFactory.registerGenerator(SyncController.GENERATOR_ID,
                new UuidBasedUniqueIdentificationGenerator(this, SESSIONS_UUID_PREF_KEY), false);
    }

    @Override
    protected void initializeLibraryDependencies() {
        ResourceBundle.initializeLocalePaks(this, R.array.locale_paks);
        ResourceExtractor.setResourcesToExtract(ResourceBundle.getActiveLocaleResources());
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX, this);
    }

    @SuppressFBWarnings("DMI_HARDCODED_ABSOLUTE_FILENAME")
    @Override
    public void initCommandLine() {
        if (!CommandLine.isInitialized()) {
            CommandLine.initFromFile(COMMAND_LINE_FILE);
        }
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
        return EmptyPKCS11AuthenticationManager.getInstance();
    }
}

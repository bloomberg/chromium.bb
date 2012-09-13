// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.testshell;

import android.app.Application;

import org.chromium.base.PathUtils;
import org.chromium.content.app.LibraryLoader;
import org.chromium.content.browser.ResourceExtractor;

/**
 * A basic test shell {@link Application}.  Handles setting up the native library and
 * loading the right resources.
 */
public class ChromiumTestShellApplication extends Application {
    private static final String TAG = ChromiumTestShellApplication.class.getCanonicalName();
    private static final String NATIVE_LIBARY = "chromiumtestshell";
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "chromiumtestshell";
    private static final String[] CHROME_MANDATORY_PAKS = {
        "chrome.pak",
        "en-US.pak",
        "resources.pak",
        "chrome_100_percent.pak",
    };

    @Override
    public void onCreate() {
        super.onCreate();

        ResourceExtractor.setMandatoryPaksToExtract(CHROME_MANDATORY_PAKS);
        LibraryLoader.setLibraryToLoad(NATIVE_LIBARY);
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
    }
}
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.app.Application;

import org.chromium.base.PathUtils;
import org.chromium.content.app.LibraryLoader;
import org.chromium.content.browser.ResourceExtractor;
import org.chromium.content.common.CommandLine;

public class AndroidWebViewTestRunnerApplication extends Application {

    /**
     * The name of the library to load.
     */
    private static final String NATIVE_LIBRARY = "webview";

    /** The minimum set of .pak files Chrome needs. */
    private static final String[] CHROME_MANDATORY_PAKS = {
        "chrome.pak", "chrome_100_percent.pak", "en-US.pak", "resources.pak",
    };

    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "webview";

    @Override
    public void onCreate() {
        super.onCreate();
        initializeApplicationParameters();
    }

    /** Handles initializing the common application parameters. */
    private static void initializeApplicationParameters() {
        CommandLine.initFromFile("/data/local/tmp/chrome-command-line");
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        // We don't need to extract any paks because for WebView, they are
        // in the system image.
        ResourceExtractor.setMandatoryPaksToExtract(CHROME_MANDATORY_PAKS);
        LibraryLoader.setLibraryToLoad(NATIVE_LIBRARY);
        LibraryLoader.loadAndInitSync();
    }
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.app.Application;

import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.content.browser.ResourceExtractor;
import org.chromium.content.common.CommandLine;

public class AndroidWebViewTestRunnerApplication extends Application {

    /** The minimum set of .pak files Chrome needs. */
    private static final String[] CHROME_MANDATORY_PAKS = {
        "chrome.pak", "chrome_100_percent.pak", "en-US.pak",
    };

    @Override
    public void onCreate() {
        super.onCreate();

        CommandLine.initFromFile("/data/local/chrome-command-line");

        ResourceExtractor.setMandatoryPaksToExtract(CHROME_MANDATORY_PAKS);
        AwBrowserProcess.loadLibrary();
        AwBrowserProcess.start(this);
    }
}

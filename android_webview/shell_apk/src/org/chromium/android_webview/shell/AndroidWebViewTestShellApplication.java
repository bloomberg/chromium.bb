// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.shell;

import android.app.Application;
import android.content.Context;

import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.test.AwTestResourceProvider;
import org.chromium.content.browser.ResourceExtractor;
import org.chromium.content.common.CommandLine;

public class AndroidWebViewTestShellApplication extends Application {

    /** The minimum set of .pak files the test runner needs. */
    private static final String[] MANDATORY_PAKS = {
        "webviewchromium.pak", "en-US.pak"
    };

    @Override
    public void onCreate() {
        super.onCreate();

        AwTestResourceProvider.registerResources(this);

        CommandLine.initFromFile("/data/local/chrome-command-line");

        ResourceExtractor.setMandatoryPaksToExtract(MANDATORY_PAKS);
        ResourceExtractor.setExtractImplicitLocaleForTesting(false);
        AwBrowserProcess.loadLibrary();
        AwBrowserProcess.start(this);
    }
}

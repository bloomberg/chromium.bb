// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.shell;

import android.app.Application;
import android.content.Context;
import android.os.Debug;
import android.util.Log;

import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.content.browser.ResourceExtractor;
import org.chromium.content.common.CommandLine;

public class AwShellApplication extends Application {

    private static final String TAG = AwShellApplication.class.getName();
    /** The minimum set of .pak files the test runner needs. */
    private static final String[] MANDATORY_PAKS = {
        "webviewchromium.pak", "en-US.pak"
    };

    @Override
    public void onCreate() {
        super.onCreate();

        AwShellResourceProvider.registerResources(this);

        CommandLine.initFromFile("/data/local/chrome-command-line");

        if (CommandLine.getInstance().hasSwitch(CommandLine.WAIT_FOR_JAVA_DEBUGGER)) {
           Log.e(TAG, "Waiting for Java debugger to connect...");
           Debug.waitForDebugger();
           Log.e(TAG, "Java debugger connected. Resuming execution.");
        }

        ResourceExtractor.setMandatoryPaksToExtract(MANDATORY_PAKS);
        ResourceExtractor.setExtractImplicitLocaleForTesting(false);
        AwBrowserProcess.loadLibrary();
        AwBrowserProcess.start(this);
    }
}

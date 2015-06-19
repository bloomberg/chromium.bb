// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.shell;

import android.os.Debug;
import android.util.Log;

import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.ResourceExtractor;
import org.chromium.base.TraceEvent;
import org.chromium.content.app.ContentApplication;

/**
 * The android_webview shell Application subclass.
 */
public class AwShellApplication extends ContentApplication {

    private static final String TAG = "AwShellApplication";
    /** The minimum set of .pak files the test runner needs. */

    @Override
    public void onCreate() {
        super.onCreate();

        AwShellResourceProvider.registerResources(this);

        CommandLine.initFromFile("/data/local/tmp/android-webview-command-line");

        if (CommandLine.getInstance().hasSwitch(BaseSwitches.WAIT_FOR_JAVA_DEBUGGER)) {
            Log.e(TAG, "Waiting for Java debugger to connect...");
            Debug.waitForDebugger();
            Log.e(TAG, "Java debugger connected. Resuming execution.");
        }

        AwBrowserProcess.loadLibrary(this);

        if (CommandLine.getInstance().hasSwitch(AwShellSwitches.ENABLE_ATRACE)) {
            Log.e(TAG, "Enabling Android trace.");
            TraceEvent.setATraceEnabled(true);
        }
    }

    @Override
    protected void initializeLibraryDependencies() {
        ResourceExtractor.setMandatoryPaksToExtract("");
        ResourceExtractor.setExtractImplicitLocaleForTesting(false);
    }

    @Override
    public void initCommandLine() {
        throw new UnsupportedOperationException();
    }
}

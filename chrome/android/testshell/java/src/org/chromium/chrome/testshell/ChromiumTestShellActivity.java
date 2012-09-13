// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.testshell;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;

import org.chromium.content.app.LibraryLoader;
import org.chromium.content.browser.ContentView;
import org.chromium.content.common.CommandLine;

/**
 * The {@link Activity} component of a basic test shell to test Chrome features.
 */
public class ChromiumTestShellActivity extends Activity {
    private static final String TAG = ChromiumTestShellActivity.class.getCanonicalName();
    private static final String COMMAND_LINE_FILE =
            "/data/local/tmp/chrome-test-shell-command-line";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (!CommandLine.isInitialized()) CommandLine.initFromFile(COMMAND_LINE_FILE);
        waitForDebuggerIfNeeded();

        LibraryLoader.loadAndInitSync();
        initializeContentViewResources();

        ContentView.initChromiumBrowserProcess(this, ContentView.MAX_RENDERERS_AUTOMATIC);
    }

    private void waitForDebuggerIfNeeded() {
        if (CommandLine.getInstance().hasSwitch(CommandLine.WAIT_FOR_JAVA_DEBUGGER)) {
            Log.e(TAG, "Waiting for Java debugger to connect...");
            android.os.Debug.waitForDebugger();
            Log.e(TAG, "Java debugger connected. Resuming execution.");
        }
    }

    private void initializeContentViewResources() {
    }
}
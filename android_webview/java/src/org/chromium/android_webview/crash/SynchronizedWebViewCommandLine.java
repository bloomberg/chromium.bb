// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.crash;

import org.chromium.base.CommandLine;
import org.chromium.base.annotations.SuppressFBWarnings;

/**
 * Class for fetching command line switches for WebView in a thread-safe way.
 */
class SynchronizedWebViewCommandLine {
    private static final Object sLock = new Object();
    private static InitState sInitialized = InitState.NOT_STARTED;
    // TODO(gsennton): this value is used in WebViewChromiumFactoryProvider as well - set it
    // somewhere where it can be read from both classes.
    private static final String WEBVIEW_COMMAND_LINE_FILE = "/data/local/tmp/webview-command-line";

    private enum InitState { NOT_STARTED, STARTED, DONE }

    /**
     * Initialize the global CommandLine using the WebView command line file.
     * This method includes IO operations - it shouldn't be performed on the main thread.
     */
    public static void initOnSeparateThread() {
        synchronized (sLock) {
            if (sInitialized != InitState.NOT_STARTED) return;
            sInitialized = InitState.STARTED;
        }
        new Thread(new Runnable() {
            @SuppressFBWarnings("DMI_HARDCODED_ABSOLUTE_FILENAME")
            @Override
            public void run() {
                CommandLine.initFromFile(WEBVIEW_COMMAND_LINE_FILE);
                synchronized (sLock) {
                    sInitialized = InitState.DONE;
                    sLock.notifyAll();
                }
            }
        }, "WebView-command-line-init-thread").start();
    }

    /**
     * Returns true if this command line contains the given switch.
     */
    public static boolean hasSwitch(String switchString) {
        synchronized (sLock) {
            while (sInitialized != InitState.DONE) {
                try {
                    sLock.wait();
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }
            }
            return CommandLine.getInstance().hasSwitch(switchString);
        }
    }
}

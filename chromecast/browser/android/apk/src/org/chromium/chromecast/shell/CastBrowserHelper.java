// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Debug;

import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chromecast.base.ChromecastConfigAndroid;
import org.chromium.content.app.ContentApplication;
import org.chromium.content.browser.BrowserStartupController;
import org.chromium.content.browser.DeviceUtils;
import org.chromium.net.NetworkChangeNotifier;

/**
 * Static, one-time initialization for the browser process.
 */
public class CastBrowserHelper {
    private static final String TAG = "CastBrowserHelper";

    public static final String COMMAND_LINE_ARGS_KEY = "commandLineArgs";

    private static boolean sIsBrowserInitialized = false;

    /**
     * Starts the browser process synchronously, returning success or failure. If the browser has
     * already started, immediately returns true without performing any more initialization.
     * This may only be called on the UI thread.
     *
     * @return whether or not the process started successfully
     */
    public static boolean initializeBrowser(Context context) {
        if (sIsBrowserInitialized) return true;

        Log.d(TAG, "Performing one-time browser initialization");

        ChromecastConfigAndroid.initializeForBrowser(context);

        // Initializing the command line must occur before loading the library.
        if (!CommandLine.isInitialized()) {
            ContentApplication.initCommandLine(context);

            if (context instanceof Activity) {
                Intent launchingIntent = ((Activity) context).getIntent();
                String[] commandLineParams = getCommandLineParamsFromIntent(launchingIntent);
                if (commandLineParams != null) {
                    CommandLine.getInstance().appendSwitchesAndArguments(commandLineParams);
                }
            }
        }

        waitForDebuggerIfNeeded();

        DeviceUtils.addDeviceSpecificUserAgentSwitch(context);

        try {
            LibraryLoader.get(LibraryProcessType.PROCESS_BROWSER).ensureInitialized();

            Log.d(TAG, "Loading BrowserStartupController...");
            BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                    .startBrowserProcessesSync(false);
            NetworkChangeNotifier.init(context);
            // Cast shell always expects to receive notifications to track network state.
            NetworkChangeNotifier.registerToReceiveNotificationsAlways();
            sIsBrowserInitialized = true;
            return true;
        } catch (ProcessInitException e) {
            Log.e(TAG, "Unable to launch browser process.", e);
            return false;
        }
    }

    private static String[] getCommandLineParamsFromIntent(Intent intent) {
        return intent != null ? intent.getStringArrayExtra(COMMAND_LINE_ARGS_KEY) : null;
    }

    private static void waitForDebuggerIfNeeded() {
        if (!CommandLine.getInstance().hasSwitch(BaseSwitches.WAIT_FOR_JAVA_DEBUGGER)) {
            return;
        }
        Log.e(TAG, "Waiting for Java debugger to connect...");
        Debug.waitForDebugger();
        Log.e(TAG, "Java debugger connected. Resuming execution.");
    }
}

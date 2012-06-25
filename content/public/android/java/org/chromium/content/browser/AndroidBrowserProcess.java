// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.app.ActivityManager;
import android.content.Context;
import android.content.res.Resources;
import android.util.Log;

import org.chromium.content.app.AppResource;
import org.chromium.content.app.ContentMain;
import org.chromium.content.app.LibraryLoader;
import org.chromium.content.browser.ContentView;
import org.chromium.content.common.CommandLine;

// NOTE: This file hasn't been fully upstreamed, please don't merge to downstream.
public class AndroidBrowserProcess {

    private static final String TAG = "BrowserProcessMain";

    // Prevents initializing the process more than once.
    private static boolean sInitialized = false;

    // Computes the actual max renderer processes used.
    private static int normalizeMaxRendererProcesses(Context context, int maxRendererProcesses) {
        if (maxRendererProcesses == MAX_RENDERERS_AUTOMATIC) {
            // We use the device's memory class to decide the maximum renderer
            // processes. For the baseline devices the memory class is 16 and we will
            // limit it to one render process. For the devices with memory class 24,
            // we allow two render processes.
            ActivityManager am = (ActivityManager)context.getSystemService(
                    Context.ACTIVITY_SERVICE);
            maxRendererProcesses = Math.max(((am.getMemoryClass() - 8) / 8), 1);
        }
        if (maxRendererProcesses > MAX_RENDERERS_LIMIT) {
            Log.w(TAG, "Excessive maxRendererProcesses value: " + maxRendererProcesses);
            return MAX_RENDERERS_LIMIT;
        }
        return Math.max(0, maxRendererProcesses);
    }

    // Automatically decide the number of renderer processes to use based on device memory class.
    public static final int MAX_RENDERERS_AUTOMATIC = -1;
    // Use single-process mode that runs the renderer on a separate thread in the main application.
    public static final int MAX_RENDERERS_SINGLE_PROCESS = 0;
    // Cap on the maximum number of renderer processes that can be requested.
    public static final int MAX_RENDERERS_LIMIT = 3;  // TODO(tedbo): Raise limit

    /**
     * Initialize the process as a ContentView host. This must be called from the main UI thread.
     * This should be called by the ContentView constructor to prepare this process for ContentView
     * use outside of the browser. In the case where ContentView is used in the browser then
     * initBrowserProcess() should already have been called and this is a no-op.
     *
     * @param context Context used to obtain the application context.
     * @param maxRendererProcesses See ContentView.enableMultiProcess().
     */
    public static void initContentViewProcess(Context context, int maxRendererProcesses) {
        genericChromiumProcessInit(context, maxRendererProcesses, false);
    }

    /**
     * Initialize the platform browser process. This must be called from the main UI thread before
     * accessing ContentView in order to treat this as a browser process.
     *
     * @param context Context used to obtain the application context.
     * @param maxRendererProcesses See ContentView.enableMultiProcess().
     */
    public static void initChromiumBrowserProcess(Context context, int maxRendererProcesses) {
        genericChromiumProcessInit(context, maxRendererProcesses, true);
    }

    /**
     * Shared implementation for the initXxx methods.
     * @param context Context used to obtain the application context
     * @param maxRendererProcesses See ContentView.enableMultiProcess()
     * @param hostIsChrome pass true if running as the system browser process.
     */
    private static void genericChromiumProcessInit(Context context, int maxRendererProcesses,
            boolean hostIsChrome) {
        if (sInitialized) {
            return;
        }
        sInitialized = true;

        // Normally Main.java will have already loaded the library asynchronously, we only
        // need to load it here if we arrived via another flow, e.g. bookmark access & sync setup.
        LibraryLoader.loadAndInitSync();

        Context appContext = context.getApplicationContext();

        // This block is inside genericChromiumProcessInit() instead
        // of initChromiumBrowserProcess() to make sure we do it once.
        // In here it is protected with the sInitialized.
        if (hostIsChrome) {
            if (nativeIsOfficialBuild() ||
                    CommandLine.getInstance().hasSwitch(CommandLine.ADD_OFFICIAL_COMMAND_LINE)) {
                Resources res = context.getResources();
                try {
                    String[] switches = res.getStringArray(AppResource.ARRAY_OFFICIAL_COMMAND_LINE);
                    CommandLine.getInstance().appendSwitchesAndArguments(switches);
                } catch (Resources.NotFoundException e) {
                    // Do nothing.  It is fine to have no command line
                    // additions for an official build.
                }
            }
        }

        int maxRenderers = normalizeMaxRendererProcesses(appContext, maxRendererProcesses);
        Log.i(TAG, "Initializing chromium process, renderers=" + maxRenderers +
                " hostIsChrome=" + hostIsChrome);

        nativeSetCommandLineFlags(maxRenderers, getPlugins(context));
        ContentMain.initApplicationContext(appContext);
        ContentMain.start();
    }

    private static String getPlugins(final Context context) {
        return "";
    }

    private static native void nativeSetCommandLineFlags(
        int maxRenderProcesses, String plugin_descriptor);

    // Is this an official build of Chrome?  Only native code knows
    // for sure.  Official build knowledge is needed very early in
    // process startup.
    private static native boolean nativeIsOfficialBuild();
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.app.ActivityManager;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Point;
import android.util.Log;
import android.view.WindowManager;
import android.view.Display;

import org.chromium.base.JNINamespace;
import org.chromium.content.app.ContentMain;
import org.chromium.content.app.LibraryLoader;
import org.chromium.content.common.CommandLine;
import org.chromium.content.common.ProcessInitException;
import org.chromium.content.R;

// NOTE: This file hasn't been fully upstreamed, please don't merge to downstream.
@JNINamespace("content")
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
    // This is currently set to account for:
    //   6: The maximum number of sandboxed processes we have available
    // - 1: The regular New Tab Page
    // - 1: The incognito New Tab Page
    // - 1: A regular incognito tab
    public static final int MAX_RENDERERS_LIMIT =
        SandboxedProcessLauncher.MAX_REGISTERED_SERVICES - 3;

    /**
     * Initialize the process as a ContentView host. This must be called from the main UI thread.
     * This should be called by the ContentView constructor to prepare this process for ContentView
     * use outside of the browser. In the case where ContentView is used in the browser then
     * initBrowserProcess() should already have been called and this is a no-op.
     *
     * @param context Context used to obtain the application context.
     * @param maxRendererProcesses See ContentView.enableMultiProcess().
     * @return Whether the process actually needed to be initialized (false if already running).
     */
    public static boolean initContentViewProcess(Context context, int maxRendererProcesses)
            throws ProcessInitException {
        return genericChromiumProcessInit(context, maxRendererProcesses, false);
    }

    /**
     * Initialize the platform browser process. This must be called from the main UI thread before
     * accessing ContentView in order to treat this as a browser process.
     *
     * @param context Context used to obtain the application context.
     * @param maxRendererProcesses See ContentView.enableMultiProcess().
     * @return Whether the process actually needed to be initialized (false if already running).
     */
    public static boolean initChromiumBrowserProcess(Context context, int maxRendererProcesses)
            throws ProcessInitException {
        return genericChromiumProcessInit(context, maxRendererProcesses, true);
    }

    /**
     * Shared implementation for the initXxx methods.
     * @param context Context used to obtain the application context
     * @param maxRendererProcesses See ContentView.enableMultiProcess()
     * @param hostIsChrome pass true if running as the system browser process.
     * @return Whether the process actually needed to be initialized (false if already running).
     */
    private static boolean genericChromiumProcessInit(Context context, int maxRendererProcesses,
            boolean hostIsChrome) throws ProcessInitException {
        if (sInitialized) return false;
        sInitialized = true;

        // Normally Main.java will have kicked this off asynchronously for Chrome. But
        // other ContentView apps like tests also need them so we make sure we've
        // extracted resources here. We can still make it a little async (wait until
        // the library is loaded).
        ResourceExtractor resourceExtractor = ResourceExtractor.get(context);
        resourceExtractor.startExtractingResources();

        // Normally Main.java will have already loaded the library asynchronously, we only
        // need to load it here if we arrived via another flow, e.g. bookmark access & sync setup.
        LibraryLoader.ensureInitialized();

        Context appContext = context.getApplicationContext();

        // This block is inside genericChromiumProcessInit() instead
        // of initChromiumBrowserProcess() to make sure we do it once.
        // In here it is protected with the sInitialized.
        if (hostIsChrome) {
            if (nativeIsOfficialBuild() ||
                    CommandLine.getInstance().hasSwitch(CommandLine.ADD_OFFICIAL_COMMAND_LINE)) {
                Resources res = context.getResources();
                try {
                    String[] switches = res.getStringArray(R.array.official_command_line);
                    CommandLine.getInstance().appendSwitchesAndArguments(switches);
                } catch (Resources.NotFoundException e) {
                    // Do nothing.  It is fine to have no command line
                    // additions for an official build.
                }
            }
        }

        // For very high resolution displays (eg. Nexus 10), set the default tile size
        // to be 512. This should be removed in favour of a generic hueristic that works
        // across all platforms and devices, once that exists: http://crbug.com/159524
        // This switches to 512 for screens containing 40 or more 256x256 tiles, such
        // that 1080p devices do not use 512x512 tiles (eg. 1920x1280 requires 37.5 tiles)
        WindowManager windowManager = (WindowManager)
                appContext.getSystemService(Context.WINDOW_SERVICE);
        Display display = windowManager.getDefaultDisplay();
        Point displaySize = new Point();
        display.getSize(displaySize);
        int numTiles = (displaySize.x * displaySize.y) / (256 * 256);
        if (numTiles >= 40
                && !CommandLine.getInstance().hasSwitch(CommandLine.DEFAULT_TILE_WIDTH)
                && !CommandLine.getInstance().hasSwitch(CommandLine.DEFAULT_TILE_HEIGHT)) {
            CommandLine.getInstance().appendSwitchWithValue(
                    CommandLine.DEFAULT_TILE_WIDTH, "512");
            CommandLine.getInstance().appendSwitchWithValue(
                    CommandLine.DEFAULT_TILE_HEIGHT, "512");
        }

        int maxRenderers = normalizeMaxRendererProcesses(appContext, maxRendererProcesses);
        Log.i(TAG, "Initializing chromium process, renderers=" + maxRenderers +
                " hostIsChrome=" + hostIsChrome);

        // Now we really need to have the resources ready.
        resourceExtractor.waitForCompletion();

        nativeSetCommandLineFlags(maxRenderers);
        ContentMain.initApplicationContext(appContext);
        int result = ContentMain.start();
        if (result > 0) throw new ProcessInitException(result);
        return true;
    }

    private static native void nativeSetCommandLineFlags(
        int maxRenderProcesses);

    // Is this an official build of Chrome?  Only native code knows
    // for sure.  Official build knowledge is needed very early in
    // process startup.
    private static native boolean nativeIsOfficialBuild();
}

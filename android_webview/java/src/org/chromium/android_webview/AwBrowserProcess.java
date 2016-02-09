// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;

import org.chromium.android_webview.policy.AwPolicyProvider;
import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.content.browser.BrowserStartupController;
import org.chromium.content.browser.ChildProcessLauncher;
import org.chromium.policy.CombinedPolicyProvider;

import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.channels.FileLock;

/**
 * Wrapper for the steps needed to initialize the java and native sides of webview chromium.
 */
public abstract class AwBrowserProcess {
    public static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "webview";

    private static final String TAG = "AwBrowserProcess";
    private static final String EXCLUSIVE_LOCK_FILE = "webview_data.lock";
    private static FileLock sExclusiveFileLock;

    /**
     * Loads the native library, and performs basic static construction of objects needed
     * to run webview in this process. Does not create threads; safe to call from zygote.
     * Note: it is up to the caller to ensure this is only called once.
     */
    public static void loadLibrary(Context context) {
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX, context);
        try {
            LibraryLoader libraryLoader = LibraryLoader.get(LibraryProcessType.PROCESS_WEBVIEW);
            libraryLoader.loadNow(context);
            // Switch the command line implementation from Java to native.
            // It's okay for the WebView to do this before initialization because we have
            // setup the JNI bindings by this point.
            libraryLoader.switchCommandLineForWebView();
        } catch (ProcessInitException e) {
            throw new RuntimeException("Cannot load WebView", e);
        }
    }

    /**
     * Configures child process launcher. This is required only if child services are used in
     * WebView.
     */
    public static void configureChildProcessLauncher(String packageName, int extraBindFlags) {
        ChildProcessLauncher.setChildProcessCreationParams(
                new ChildProcessLauncher.ChildProcessCreationParams(packageName, extraBindFlags,
                        LibraryProcessType.PROCESS_WEBVIEW_CHILD));
    }

    /**
     * Starts the chromium browser process running within this process. Creates threads
     * and performs other per-app resource allocations; must not be called from zygote.
     * Note: it is up to the caller to ensure this is only called once.
     * @param context The Android application context
     */
    public static void start(final Context context) {
        tryObtainingDataDirLockOrDie(context);
        // We must post to the UI thread to cover the case that the user
        // has invoked Chromium startup by using the (thread-safe)
        // CookieManager rather than creating a WebView.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // The policies are used by browser startup, so we need to register the policy
                // providers before starting the browser process. This only registers java objects
                // and doesn't need the native library.
                CombinedPolicyProvider.get().registerProvider(new AwPolicyProvider(context));

                try {
                    BrowserStartupController.get(context, LibraryProcessType.PROCESS_WEBVIEW)
                            .startBrowserProcessesSync(!CommandLine.getInstance().hasSwitch(
                                            AwSwitches.WEBVIEW_SANDBOXED_RENDERER));
                } catch (ProcessInitException e) {
                    throw new RuntimeException("Cannot initialize WebView", e);
                }
            }
        });
    }

    private static void tryObtainingDataDirLockOrDie(Context context) {
        String dataPath = PathUtils.getDataDirectory(context);
        File lockFile = new File(dataPath, EXCLUSIVE_LOCK_FILE);
        boolean success = false;
        try {
            // Note that the file is not closed intentionally.
            RandomAccessFile file = new RandomAccessFile(lockFile, "rw");
            sExclusiveFileLock = file.getChannel().tryLock();
            success = sExclusiveFileLock != null;
        } catch (IOException e) {
            Log.w(TAG, "Failed to create lock file " + lockFile, e);
        }
        if (!success) {
            Log.w(TAG, "The app may have another WebView opened in a separate process. "
                    + "This is not recommended and may stop working in future versions.");
        }
    }
}

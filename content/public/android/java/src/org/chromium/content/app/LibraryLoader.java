// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.app;

import android.os.AsyncTask;
import android.os.Handler;
import android.text.TextUtils;
import android.util.Log;

import org.chromium.base.JNINamespace;
import org.chromium.content.common.CommandLine;
import org.chromium.content.common.ProcessInitException;
import org.chromium.content.common.TraceEvent;

/**
 * This class provides functionality to load and register the native library.
 * In most cases, users will call ensureInitialized() from their main thread
 * (only) which ensures a post condition that the library is loaded,
 * initialized, and ready to use.
 * Optionally, an application may optimize startup be calling loadNow early on,
 * from a background thread, and then on completion of that method it must call
 * ensureInitialized() on the main thread before it tries to access any native
 * code.
 */
@JNINamespace("content")
public class LibraryLoader {
    private static final String TAG = "LibraryLoader";

    private static String sLibrary = null;

    // This object's lock guards sLoaded assignment and also the library load.
    private static Object sLoadedLock = new Object();
    private static Boolean sLoaded = false;

    private static boolean sInitialized = false;


    /**
     * Sets the library name that is to be loaded.  This must be called prior to the library being
     * loaded the first time.
     *
     * @param library The name of the library to be loaded (without the lib prefix).
     */
    public static void setLibraryToLoad(String library) {
        if (TextUtils.equals(sLibrary, library)) return;

        assert !sLoaded : "Setting the library must happen before load is called.";
        sLibrary = library;
    }

    /**
     * @return The name of the native library set to be loaded.
     */
    public static String getLibraryToLoad() {
        return sLibrary;
    }

    /**
     *  This method blocks until the library is fully loaded and initialized;
     *  must be called on the thread that the native will call its "main" thread.
     */
    public static void ensureInitialized() throws ProcessInitException {
        checkThreadUsage();
        if (sInitialized) {
            // Already initialized, nothing to do.
            return;
        }
        loadNow();
        initializeOnMainThread();
    }

    /**
     * Loads the library and blocks until the load completes. The caller is responsible
     * for subsequently calling ensureInitialized().
     * May be called on any thread, but should only be called once. Note the thread
     * this is called on will be the thread that runs the native code's static initializers.
     * See the comment in doInBackground() for more considerations on this.
     *
     * @return Whether the native library was successfully loaded.
     */
    public static void loadNow() {
        if (sLibrary == null) {
            assert false : "No library specified to load.  Call setLibraryToLoad before first.";
        }
        synchronized (sLoadedLock) {
            if (!sLoaded) {
                assert !sInitialized;
                Log.i(TAG, "loading: " + sLibrary);
                System.loadLibrary(sLibrary);
                Log.i(TAG, "loaded: " + sLibrary);
                sLoaded = true;
            }
        }
    }

    /**
     * initializes the library here and now: must be called on the thread that the
     * native will call its "main" thread. The library must have previously been
     * loaded with loadNow.
     * @param initCommandLine The command line arguments that native command line will
     * be initialized with.
     */
    static void initializeOnMainThread(String[] initCommandLine) throws ProcessInitException {
        checkThreadUsage();
        if (sInitialized) {
            return;
        }
        int resultCode = nativeLibraryLoadedOnMainThread(initCommandLine);
        if (resultCode != 0) {
            Log.e(TAG, "error calling nativeLibraryLoadedOnMainThread");
            throw new ProcessInitException(resultCode);
        }
        // From this point on, native code is ready to use and checkIsReady()
        // shouldn't complain from now on (and in fact, it's used by the
        // following calls).
        sInitialized = true;
        CommandLine.enableNativeProxy();
        TraceEvent.setEnabledToMatchNative();
    }

    static private void initializeOnMainThread() throws ProcessInitException {
        checkThreadUsage();
        if (!sInitialized) {
            initializeOnMainThread(CommandLine.getJavaSwitchesOrNull());
        }
    }

    private LibraryLoader() {
    }

    // This asserts that calls to ensureInitialized() will happen from the
    // same thread.
    private static Object sCheckThreadLock = new Object();
    private static Thread sMyThread;
    private static void checkThreadUsage() {
        Thread currentThread = Thread.currentThread();
        synchronized (sCheckThreadLock) {
            if (sMyThread == null) {
                sMyThread = currentThread;
            } else {
                if (sMyThread != currentThread) {
                    Log.e(TAG, "Threading violation detected. My thread=" + sMyThread + " id=" +
                            sMyThread.getId() + " but I'm being accessed from thread=" +
                          currentThread + " id=" + currentThread.getId());
                    assert false;
                }
            }
        }
    }

    // This is the only method that is registered during System.loadLibrary, as it
    // may happen on a different thread. We then call it on the main thread to register
    // everything else.
    // Return 0 on success, otherwise return the error code from
    // content/public/common/result_codes.h.
    private static native int nativeLibraryLoadedOnMainThread(String[] initCommandLine);
}

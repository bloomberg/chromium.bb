// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.app;

import android.text.TextUtils;
import android.util.Log;

import org.chromium.base.JNINamespace;
import org.chromium.content.common.CommandLine;
import org.chromium.content.common.ProcessInitException;
import org.chromium.content.common.ResultCodes;
import org.chromium.content.common.TraceEvent;

/**
 * This class provides functionality to load and register the native library.
 * Callers are allowed to separate loading the library from initializing it.
 * This may be an advantage for Android Webview, where the library can be loaded
 * by the zygote process, but then needs per process initialization after the
 * application processes are forked from the zygote process.
 *
 * The library may be loaded and initialized from any thread. Synchronization
 * primitives are used to ensure that overlapping requests from different
 * threads are handled sequentially.
 */
@JNINamespace("content")
public class LibraryLoader {
    private static final String TAG = "LibraryLoader";

    private static String sLibrary = null;

    // This object's lock guards sLoaded assignment and also the library load.
    private static Object sLoadLock = new Object();
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
     *  This method blocks until the library is fully loaded and initialized.
     */
    public static void ensureInitialized() throws ProcessInitException {
        synchronized (sLoadLock) {
            if (sInitialized) {
                // Already initialized, nothing to do.
                return;
            }
            loadAlreadyLocked();
            initializeAlreadyLocked(CommandLine.getJavaSwitchesOrNull());
        }
    }


    /**
     * Loads the library and blocks until the load completes. The caller is responsible
     * for subsequently calling ensureInitialized().
     * May be called on any thread, but should only be called once. Note the thread
     * this is called on will be the thread that runs the native code's static initializers.
     * See the comment in doInBackground() for more considerations on this.
     *
     * @throws ProcessInitException if the native library failed to load.
     */
    public static void loadNow() throws ProcessInitException {
        synchronized (sLoadLock) {
            loadAlreadyLocked();
        }
    }


    /**
     * initializes the library here and now: must be called on the thread that the
     * native will call its "main" thread. The library must have previously been
     * loaded with loadNow.
     * @param initCommandLine The command line arguments that native command line will
     * be initialized with.
     */
    static void initialize(String[] initCommandLine) throws ProcessInitException {
        synchronized (sLoadLock) {
            initializeAlreadyLocked(initCommandLine);
        }
    }


    private static void loadAlreadyLocked() throws ProcessInitException {
        if (sLibrary == null) {
            assert false : "No library specified to load.  Call setLibraryToLoad before first.";
        }
        try {
            if (!sLoaded) {
                assert !sInitialized;
                Log.i(TAG, "loading: " + sLibrary);
                System.loadLibrary(sLibrary);
                Log.i(TAG, "loaded: " + sLibrary);
                sLoaded = true;
            }
        } catch (UnsatisfiedLinkError e) {
            throw new ProcessInitException(ResultCodes.RESULT_CODE_NATIVE_LIBRARY_LOAD_FAILED, e);
        }
    }


    private static void initializeAlreadyLocked(String[] initCommandLine)
            throws ProcessInitException {
        if (sInitialized) {
            return;
        }
        int resultCode = nativeLibraryLoaded(initCommandLine);
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

    // This is the only method that is registered during System.loadLibrary. We then call it
    // to register everything else.
    // Return 0 on success, otherwise return the error code from
    // content/public/common/result_codes.h.
    private static native int nativeLibraryLoaded(String[] initCommandLine);
}

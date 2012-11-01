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

    // This object's lock guards its assignment and also the library load.
    private static Boolean sLoaded = false;

    private static boolean sInitialized = false;

    private static AsyncTask<Void, Void, Boolean> sAsyncLoader;

    /**
     * Callback for handling loading of the native library.
     *
     * <p> The callback methods will always be triggered on the UI thread.
     */
    @Deprecated
    public static interface Callback {
        /**
         * Called when loading the native library is successful.
         */
        void onSuccess();

        /**
         * Called when loading the native library fails.
         */
        void onFailure();
    }

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

    @Deprecated
    public static void loadAndInitSync() {
        // TODO(joth): remove in next patch.
        ensureInitialized();
    }

    /**
     *  This method blocks until the library is fully loaded and initialized;
     *  must be called on the thread that the native will call its "main" thread.
     */
    public static void ensureInitialized() {
        checkThreadUsage();
        if (sInitialized) {
            // Already initialized, nothing to do.
            return;
        }
        if (sAsyncLoader != null) {
            // Async initialization in progress, wait.
            waitForAsyncInitialized();
            return;
        }
        loadNow();
        initializeOnMainThread();
    }

    /**
     *  Block until the library is fully initialized.
     *  Must be called on the thread that the native will call its "main" thread.
     */
    private static void waitForAsyncInitialized() {
        checkThreadUsage();
        if (sInitialized) {
            // Already initialized.
            return;
        }
        synchronized(LibraryLoader.class) {
            try {
                while (!sLoaded) {
                    LibraryLoader.class.wait();
                }
                // If the UI thread blocked waiting for the task it will already
                // have handled the library load completion, so don't duplicate that work here.
            } catch (InterruptedException e) {
            }
        }
        initializeOnMainThread();
    }

    /**
     *  Kicks off an asynchronous library load, and will asynchronously initialize the
     *  library when that completes.
     *  Must be called on the thread that the native will call its "main" thread.
     */
    @Deprecated
    public static void loadAndInitAsync(final Callback onLoadedListener) {
        checkThreadUsage();
        if (sInitialized) {
            // Already initialized, post our Runnable if needed.
            if (onLoadedListener != null) {
                new Handler().post(new Runnable() {
                    @Override
                    public void run() {
                        onLoadedListener.onSuccess();
                    }
                });
            }
            return;
        }
        sAsyncLoader = new AsyncTask<Void, Void, Boolean>() {
            @Override
            public Boolean doInBackground(Void... voids) {
                // We're loading the .so in a background thread. Potentially, this
                // can break native code that relies on static initializers using
                // thread local storage, as the library would normally load in the
                // main thread. If do we hit such cases we should remove those static
                // initializers, as we chrome has banned them.
                // (Worst case, we can go back to just warming up the file in the system
                // cache here and do the actual loading in onPostExecute().)
                try {
                  loadNow();
                  return true;
                } catch (UnsatisfiedLinkError e) {
                  return false;
                }
            }

            @Override
            protected void onPostExecute(Boolean result) {
                if (result) {
                    initializeOnMainThread();
                    if (onLoadedListener != null) onLoadedListener.onSuccess();
                } else {
                    if (onLoadedListener != null) onLoadedListener.onFailure();
                }

            }
        };
        sAsyncLoader.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * @throws UnsatisfiedLinkError if the library is not yet initialized.
     */
    public static void checkIsReady() {
        if (!sInitialized) {
            throw new UnsatisfiedLinkError(sLibrary + " is not initialized");
        }
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
        synchronized(sLoaded) {
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
    static void initializeOnMainThread(String[] initCommandLine) {
        checkThreadUsage();
        if (sInitialized) {
            return;
        }
        if (!nativeLibraryLoadedOnMainThread(initCommandLine)) {
            Log.e(TAG, "error calling nativeLibraryLoadedOnMainThread");
            throw new UnsatisfiedLinkError();
        }
        // From this point on, native code is ready to use and checkIsReady()
        // shouldn't complain from now on (and in fact, it's used by the
        // following calls).
        sInitialized = true;
        CommandLine.enableNativeProxy();
        TraceEvent.setEnabledToMatchNative();
    }

    static private void initializeOnMainThread() {
        checkThreadUsage();
        if (!sInitialized) {
            initializeOnMainThread(CommandLine.getJavaSwitchesOrNull());
        }
    }

    private LibraryLoader() {
    }

    // This asserts that calls to ensureInitialized() will happen from the
    // same thread.
    private static Thread sMyThread;
    private static void checkThreadUsage() {
        Thread currentThread = java.lang.Thread.currentThread();
        if (sMyThread == null) {
            sMyThread = currentThread;
        } else {
            if (sMyThread != currentThread) {
                Log.e(TAG, "Threading violation detected. My thread=" + sMyThread +
                    " but I'm being accessed from thread=" + currentThread);
                assert false;
            }
        }
    }

    // This is the only method that is registered during System.loadLibrary, as it
    // may happen on a different thread. We then call it on the main thread to register
    // everything else.
    private static native boolean nativeLibraryLoadedOnMainThread(String[] initCommandLine);
}

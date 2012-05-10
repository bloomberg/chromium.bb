// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.os.AsyncTask;
import android.os.Handler;
import android.text.TextUtils;
import android.util.Log;

// This class provides functionality to:
// - synchronously load and register the native library. This is used by callers
// that can't do anything useful without the native side.
// - asynchronously load and register the native library. This is used by callers
// that can do more work in the java-side, and let a separate thread do all the
// file IO and library loading.
public class LibraryLoader {
    private static final String TAG = "LibraryLoader";

    /* TODO(jrg): resolve up and downstream discrepancy; there is no
     * upstream libchromeview.so */
    private static String sLibrary = "chromeview";

    private static boolean sLoaded = false;

    private static boolean sInitialized = false;

    private static AsyncTask<Void, Void, Boolean> sAsyncLoader;

    /**
     * Callback for handling loading of the native library.
     *
     * <p> The callback methods will always be triggered on the UI thread.
     */
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
     *  This method blocks until the library is fully loaded and initialized;
     *  must be called on the thread that the native will call its "main" thread.
     */
    public static void loadAndInitSync() {
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
                return loadNow();
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
     * for subsequently calling initialize().
     * May be called on any thread, but should only be called once. Note the thread
     * this is called on will be the thread that runs the native code's static initializers.
     * See the comment in doInBackground() for more considerations on this.
     *
     * @return Whether the native library was successfully loaded.
     */
    static boolean loadNow() {
        assert !sInitialized;
        try {
            Log.i(TAG, "loading: " + sLibrary);
            System.loadLibrary(sLibrary);
            Log.i(TAG, "loaded: " + sLibrary);
            synchronized(LibraryLoader.class) {
                sLoaded = true;
                LibraryLoader.class.notifyAll();
            }
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "error loading: " + sLibrary, e);
            return false;
        }
        return true;
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

    // The public API of this class is meant to be used from a single
    // thread. Internally, we may bounce to a separate thread to actually
    // load the library.
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
    // happens on a different thread. We then call it on the main thread to register
    // everything else.
    private static native boolean nativeLibraryLoadedOnMainThread(String[] initCommandLine);
}

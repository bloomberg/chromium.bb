// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp;

import android.content.Context;
import android.os.Handler;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ResourceExtractor;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;

/**
 * Asynchronously loads and registers the native libraries associated with Blimp.
 */
@JNINamespace("blimp::client")
public final class BlimpLibraryLoader {
    /**
     * A callback interface that is notified with the native library load results.
     */
    public interface Callback {
        /**
         * Called when the load attempt is finished (regardless of whether or not it was
         * successful).
         * @param success Whether or not the native library was successfully loaded.
         */
        void onStartupComplete(boolean success);
    }

    /**
     * Whether or not a call to {@link #startAsync(Context, Callback)} is/has actually attempted to
     * load the native library.
     */
    private static boolean sLoadAttempted = false;

    /** If not {@code null} the result of a load attempt. */
    private static Boolean sLibraryLoadResult;

    /**
     * A list of {@link Callback} instances that still need to be notified of the result of the
     * initial call to {@link #startAsync(Context, Callback)}.
     */
    private static ObserverList<Callback> sOutstandingCallbacks = new ObserverList<Callback>();

    /**
     * Disallow instantiation of this class.
     */
    private BlimpLibraryLoader() {}

    /**
     * Starts asynchronously loading and registering the native libraries.  If this is called more
     * than once, only the first caller will actually load the library.  The subsequent calls will
     * wait for the first call to finish and notify their {@link BlimpLibraryLoader.Callback}
     * instances accordingly.  Any calls to this after the library has finished loading will just
     * have the initial load result posted back to {@code callback}.
     * @param context               A {@link Context} object.
     * @param callback              A {@link BlimpLibraryLoader.Callback} to be notified upon
     *                              completion.
     * @throws ProcessInitException
     */
    public static void startAsync(final Context context, final Callback callback)
            throws ProcessInitException {
        ThreadUtils.assertOnUiThread();

        // Save the callback to be notified once loading and initializiation is one.
        sOutstandingCallbacks.addObserver(callback);

        if (sLibraryLoadResult != null) {
            // The library is already loaded, notify {@code callback} and skip the rest of the
            // loading steps.
            notifyCallbacksAndClear();
            return;
        }

        // If we're already in the process of loading, skip this call.  Otherwise mark that we are
        // loading and do the actual load.  Subsequent calls won't run the load steps, but will wait
        // for this load to finish.
        if (sLoadAttempted) return;
        sLoadAttempted = true;

        ResourceExtractor extractor = ResourceExtractor.get(context);
        extractor.startExtractingResources();
        LibraryLoader.get(LibraryProcessType.PROCESS_BROWSER).ensureInitialized(context);

        extractor.addCompletionCallback(new Runnable() {
            @Override
            public void run() {
                ContextUtils.initApplicationContext(context.getApplicationContext());
                new Handler().post(new Runnable() {
                    @Override
                    public void run() {
                        // Only run nativeStartBlimp if we properly initialized native.
                        boolean startResult = nativeStartBlimp();
                        sLibraryLoadResult = new Boolean(startResult);

                        // Notify any oustanding callers to #startAsync().
                        notifyCallbacksAndClear();
                    }
                });
            }
        });
    }

    private static void notifyCallbacksAndClear() {
        for (Callback callback : sOutstandingCallbacks) {
            notifyCallback(callback);
        }

        // Clear the callback list so we don't hold onto references to callers.
        sOutstandingCallbacks.clear();
    }

    private static void notifyCallback(final Callback callback) {
        new Handler().post(new Runnable() {
            @Override
            public void run() {
                ThreadUtils.assertOnUiThread();
                callback.onStartupComplete(sLibraryLoadResult);
            }
        });
    }

    // Native methods.
    private static native boolean nativeStartBlimp();
}

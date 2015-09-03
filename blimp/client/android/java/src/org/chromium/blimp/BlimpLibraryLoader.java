// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp;

import android.content.Context;
import android.os.Handler;

import org.chromium.base.ResourceExtractor;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;

/**
 * Asynchronously loads and registers the native libraries associated with Blimp.
 */
@JNINamespace("blimp")
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
     * Disallow instantiation of this class.
     */
    private BlimpLibraryLoader() {}

    /**
     * Starts asynchronously loading and registering the native libraries.
     * @param context               A {@link Context} object.
     * @param callback              A {@link BlimpLibraryLoader.Callback} to be notified upon
     *                              completion.
     * @throws ProcessInitException
     */
    public static void startAsync(final Context context, final Callback callback)
            throws ProcessInitException {
        ResourceExtractor extractor = ResourceExtractor.get(context);
        extractor.startExtractingResources();

        LibraryLoader.get(LibraryProcessType.PROCESS_BROWSER).ensureInitialized(context);

        extractor.addCompletionCallback(new Runnable() {
            @Override
            public void run() {
                final boolean initResult = nativeInitializeBlimp(context.getApplicationContext());
                new Handler().post(new Runnable() {
                    @Override
                    public void run() {
                        // Only run nativeStartBlimp if we properly initialized native.
                        boolean startResult = initResult && nativeStartBlimp();
                        if (callback != null) callback.onStartupComplete(startResult);
                    }
                });
            }
        });
    }

    // Native methods.
    private static native boolean nativeInitializeBlimp(Context context);
    private static native boolean nativeStartBlimp();
}

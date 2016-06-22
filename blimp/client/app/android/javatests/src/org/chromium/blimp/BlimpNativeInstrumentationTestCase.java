// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp;

import android.test.InstrumentationTestCase;

import org.chromium.base.library_loader.ProcessInitException;

import java.util.concurrent.Semaphore;

/**
 * Base class for loading native library in tests. The setUp() methods must be invoked, and
 * subclasses can call {@link #waitUntilNativeIsReady()} in the start of each test-method.
 */
public class BlimpNativeInstrumentationTestCase extends InstrumentationTestCase {
    private final Semaphore mNativeReadySemaphore = new Semaphore(0);
    private boolean mSuccess = false;

    @Override
    public void setUp() throws ProcessInitException {
        BlimpLibraryLoader.startAsync(
                getInstrumentation().getTargetContext(), new BlimpLibraryLoader.Callback() {
                    public void onStartupComplete(boolean success) {
                        mSuccess = success;
                        mNativeReadySemaphore.release();
                    }
                });
    }

    /**
     * Blocks until the native library startup is complete. If the startup dit not complete
     * successfully, this method throws a RuntimeException, otherwise it does nothing.
     * This method should be called in the start of any test method that requires native to be
     * successfully loaded.
     */
    protected final void waitUntilNativeIsReady() throws InterruptedException {
        mNativeReadySemaphore.acquire();
        if (!mSuccess) throw new RuntimeException("Native startup failed");
    }
}

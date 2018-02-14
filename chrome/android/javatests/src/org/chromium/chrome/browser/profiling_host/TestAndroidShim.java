// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiling_host;

import org.chromium.base.annotations.MainDex;

/**
 * Provides direct access to test_android_shim, which in turn forwards to
 * ProfilingTestDriver. Only used for testing.
 */
@MainDex
public class TestAndroidShim {
    public TestAndroidShim() {
        mNativeTestAndroidShim = nativeInit();
    }

    /**
     *  When |dynamicallyStartProfiling| is true, the test harness is
     *  responsible for starting profiling for the relevant processes.
     *  When |pseudoStacks| is true, the stacks use trace-event based stacks
     *  rather than native stacks.
     */
    public boolean runTestForMode(String mode, boolean dynamicallyStartProfiling, String stackMode,
            boolean shouldSample, boolean sampleEverything) {
        return nativeRunTestForMode(mNativeTestAndroidShim, mode, dynamicallyStartProfiling,
                stackMode, shouldSample, sampleEverything);
    }

    /**
     * Clean up the C++ side of this class.
     * After the call, this class instance shouldn't be used.
     */
    public void destroy() {
        if (mNativeTestAndroidShim != 0) {
            nativeDestroy(mNativeTestAndroidShim);
            mNativeTestAndroidShim = 0;
        }
    }

    private long mNativeTestAndroidShim;
    private native long nativeInit();
    private native void nativeDestroy(long nativeTestAndroidShim);
    private native boolean nativeRunTestForMode(long nativeTestAndroidShim, String mode,
            boolean dynamicallyStartProfiling, String stackMode, boolean shouldSample,
            boolean sampleEverything);
}

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test;

import android.test.InstrumentationTestCase;

/**
 * Test extension that adds support for loading and dealing with native libraries.
 */
public class NativeLibraryTestBase extends InstrumentationTestCase {
    private final NativeLibraryTestCommon mTestCommon = new NativeLibraryTestCommon();

    /**
     * Loads the native library on the activity UI thread (must not be called from the UI thread).
     */
    @SuppressWarnings("deprecation")
    public void loadNativeLibraryNoBrowserProcess() {
        mTestCommon.handleNativeInitialization(false, getInstrumentation());
    }

    /**
     * Loads the native library on the activity UI thread (must not be called from the UI thread).
     * After loading the library, this will initialize the browser process.
     */
    @SuppressWarnings("deprecation")
    public void loadNativeLibraryAndInitBrowserProcess() {
        mTestCommon.handleNativeInitialization(true, getInstrumentation());
    }
}

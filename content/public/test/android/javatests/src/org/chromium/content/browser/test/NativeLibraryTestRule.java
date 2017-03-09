// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test;

import android.support.test.InstrumentationRegistry;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

/**
 * TestRule that adds support for loading and dealing with native libraries.
 *
 * NativeLibraryTestRule does not interact with any Activity.
 */
public class NativeLibraryTestRule implements TestRule {
    private final NativeLibraryTestCommon mTestCommon = new NativeLibraryTestCommon();

    /**
     * Loads the native library on the activity UI thread (must not be called from the UI thread).
     */
    public void loadNativeLibraryNoBrowserProcess() {
        mTestCommon.handleNativeInitialization(false, InstrumentationRegistry.getInstrumentation());
    }

    /**
     * Loads the native library on the activity UI thread (must not be called from the UI thread).
     * After loading the library, this will initialize the browser process.
     */
    public void loadNativeLibraryAndInitBrowserProcess() {
        mTestCommon.handleNativeInitialization(true, InstrumentationRegistry.getInstrumentation());
    }

    @Override
    public Statement apply(Statement base, Description description) {
        return base;
    }
}

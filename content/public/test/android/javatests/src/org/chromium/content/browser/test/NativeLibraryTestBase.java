// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test;

import android.test.InstrumentationTestCase;

import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.content.browser.BrowserStartupController;
import org.chromium.content.browser.test.util.ApplicationUtils;

/**
 * Test extension that adds support for loading and dealing with native libraries.
 */
public class NativeLibraryTestBase extends InstrumentationTestCase {
    /**
     * Loads the native library on the activity UI thread (must not be called from the UI thread).
     */
    public void loadNativeLibraryNoBrowserProcess() {
        handleNativeInitialization(false);
    }

    /**
     * Loads the native library on the activity UI thread (must not be called from the UI thread).
     * After loading the library, this will initialize the browser process.
     */
    public void loadNativeLibraryAndInitBrowserProcess() {
        handleNativeInitialization(true);
    }

    private void handleNativeInitialization(final boolean initBrowserProcess) {
        assertFalse(ThreadUtils.runningOnUiThread());

        try {
            ApplicationUtils.waitForLibraryDependencies(getInstrumentation());
        } catch (InterruptedException e) {
            fail("Library dependencies were never initialized.");
        }

        // LibraryLoader is not in general multithreaded; as other InstrumentationTestCase code
        // (specifically, ChromeBrowserProvider) uses it from the main thread we must do
        // likewise.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                nativeInitialization(initBrowserProcess);
            }
        });
    }

    private void nativeInitialization(boolean initBrowserProcess) {
        if (initBrowserProcess) {
            try {
                BrowserStartupController.get(getInstrumentation().getTargetContext(),
                        LibraryProcessType.PROCESS_BROWSER).startBrowserProcessesSync(false);
            } catch (ProcessInitException e) {
                throw new Error(e);
            }
        } else {
            try {
                LibraryLoader.get(LibraryProcessType.PROCESS_BROWSER)
                        .ensureInitialized(getInstrumentation().getTargetContext());
            } catch (ProcessInitException e) {
                throw new Error(e);
            }
        }
    }
}

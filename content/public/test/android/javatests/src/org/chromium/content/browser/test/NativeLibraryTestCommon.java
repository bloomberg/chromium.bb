// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test;

import android.app.Instrumentation;

import org.junit.Assert;

import org.chromium.base.PathUtils;
import org.chromium.base.ResourceExtractor;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.content.browser.BrowserStartupController;

class NativeLibraryTestCommon {
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "content";

    void handleNativeInitialization(
            final boolean initBrowserProcess, Instrumentation instrumentation) {
        Assert.assertFalse(ThreadUtils.runningOnUiThread());

        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);

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

    void nativeInitialization(boolean initBrowserProcess) {
        if (initBrowserProcess) {
            try {
                // Extract compressed resource paks.
                ResourceExtractor resourceExtractor = ResourceExtractor.get();
                resourceExtractor.startExtractingResources();
                resourceExtractor.waitForCompletion();

                BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                        .startBrowserProcessesSync(false);
            } catch (ProcessInitException e) {
                throw new Error(e);
            }
        } else {
            try {
                LibraryLoader.get(LibraryProcessType.PROCESS_BROWSER).ensureInitialized();
            } catch (ProcessInitException e) {
                throw new Error(e);
            }
        }
    }
}

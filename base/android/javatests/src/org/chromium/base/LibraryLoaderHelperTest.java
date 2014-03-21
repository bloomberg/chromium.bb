// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.Context;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.library_loader.LibraryLoaderHelper;

import java.io.File;

/**
 * Test class for the native library hack.
 */
public class LibraryLoaderHelperTest extends InstrumentationTestCase {
    private static final String TAG = "LibraryLoaderHelperTest";

    @Override
    public void setUp() throws Exception {
        Context context = getInstrumentation().getTargetContext();
        LibraryLoaderHelper.deleteWorkaroundLibrariesSynchronously(context);
    }

    @MediumTest
    public void testLoadNativeLibraries() {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                Context context = getInstrumentation().getTargetContext();
                File libDir = LibraryLoaderHelper.getWorkaroundLibDir(context);
                assertTrue(libDir.exists());
                assertTrue(libDir.isDirectory());
                assertEquals(libDir.list().length, 0);

                assertTrue(
                    LibraryLoaderHelper.loadNativeLibrariesUsingWorkaroundForTesting(
                        context));

                assertTrue(libDir.list().length > 0);
            }
        });
    }

    @Override
    public void tearDown() throws Exception {
        Context context = getInstrumentation().getTargetContext();
        LibraryLoaderHelper.deleteWorkaroundLibrariesSynchronously(context);
        super.tearDown();
    }
}

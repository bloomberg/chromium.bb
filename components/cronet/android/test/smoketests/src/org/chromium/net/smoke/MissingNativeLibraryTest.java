// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.smoke;

import android.support.test.filters.SmallTest;

import org.chromium.net.CronetEngine;
import org.chromium.net.ExperimentalCronetEngine;

/**
 *  Tests scenarios when the native shared library file is missing in the APK or was built for a
 *  wrong architecture.
 */
public class MissingNativeLibraryTest extends CronetSmokeTestCase {
    /**
     * If the ".so" file is missing, instantiating the Cronet engine should throw an exception.
     */
    @SmallTest
    public void testExceptionWhenSoFileIsAbsent() throws Exception {
        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(getContext());
        try {
            builder.build();
            fail("Expected exception since the shared library '.so' file is absent");
        } catch (Throwable t) {
            // Find the root cause.
            while (t.getCause() != null) {
                t = t.getCause();
            }
            assertEquals(UnsatisfiedLinkError.class, t.getClass());
        }
    }

    /**
     * Tests the enableLegacyMode API that allows the embedder to force JavaCronetEngine
     * instantiation even when the native Cronet engine implementation is available.
     */
    @SmallTest
    public void testEnableLegacyMode() throws Exception {
        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(getContext());
        builder.enableLegacyMode(true);
        CronetEngine engine = builder.build();
        assertJavaEngine(engine);
    }
}

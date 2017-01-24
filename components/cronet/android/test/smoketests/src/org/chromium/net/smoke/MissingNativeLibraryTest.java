// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.smoke;

import android.support.test.filters.SmallTest;

import org.chromium.net.CronetEngine;
import org.chromium.net.CronetProvider;
import org.chromium.net.ExperimentalCronetEngine;

import java.util.List;

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
     * Tests the embedder ability to select Java (platform) based implementation when
     * the native library is missing or doesn't load for some reason,
     */
    @SmallTest
    public void testForceChoiceOfJavaEngine() throws Exception {
        List<CronetProvider> availableProviders = CronetProvider.getAllProviders(getContext());
        boolean foundNativeProvider = false;
        CronetProvider platformProvider = null;
        for (CronetProvider provider : availableProviders) {
            assertTrue(provider.isEnabled());
            if (provider.getName().equals(CronetProvider.PROVIDER_NAME_APP_PACKAGED)) {
                foundNativeProvider = true;
            } else if (provider.getName().equals(CronetProvider.PROVIDER_NAME_FALLBACK)) {
                platformProvider = provider;
            }
        }

        assertTrue("Unable to find the native cronet provider", foundNativeProvider);
        assertNotNull("Unable to find the platform cronet provider", platformProvider);

        CronetEngine.Builder builder = platformProvider.createBuilder();
        CronetEngine engine = builder.build();
        assertJavaEngine(engine);

        assertTrue("It should be always possible to cast the created builder to"
                        + " ExperimentalCronetEngine.Builder",
                builder instanceof ExperimentalCronetEngine.Builder);

        assertTrue("It should be always possible to cast the created engine to"
                        + " ExperimentalCronetEngine.Builder",
                engine instanceof ExperimentalCronetEngine);
    }
}

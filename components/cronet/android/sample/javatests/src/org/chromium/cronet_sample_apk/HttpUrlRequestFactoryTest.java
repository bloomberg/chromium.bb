// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_sample_apk;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.net.HttpUrlRequestFactory;
import org.chromium.net.HttpUrlRequestFactoryConfig;

import java.util.regex.Pattern;

/**
 * Tests for {@link ChunkedWritableByteChannel}
 */
public class HttpUrlRequestFactoryTest extends InstrumentationTestCase {
    @SmallTest
    @Feature({"Cronet"})
    public void testCreateFactory() {
        HttpUrlRequestFactoryConfig config = new HttpUrlRequestFactoryConfig();
        HttpUrlRequestFactory factory = HttpUrlRequestFactory.createFactory(
                getInstrumentation().getContext(), config);
        assertNotNull("Factory should be created", factory);
        assertTrue("Factory should be Chromium/n.n.n.n/r",
                   Pattern.matches("Chromium/\\d+\\.\\d+\\.\\d+\\.\\d+@\\d+",
                   factory.getName()));
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testCreateLegacyFactory() {
        HttpUrlRequestFactoryConfig config = new HttpUrlRequestFactoryConfig();
        config.enableLegacyMode(true);

        HttpUrlRequestFactory factory = HttpUrlRequestFactory.createFactory(
                getInstrumentation().getContext(), config);
        assertNotNull("Factory should be created", factory);
        assertTrue("Factory should be HttpUrlConnection/n.n.n.n@r",
                   Pattern.matches("HttpUrlConnection/\\d+\\.\\d+\\.\\d+\\.\\d+@\\d+",
                   factory.getName()));
    }
}

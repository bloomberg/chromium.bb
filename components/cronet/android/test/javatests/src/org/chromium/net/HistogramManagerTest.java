// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;

import java.util.Arrays;

/**
 * Test HistogramManager.
 */
public class HistogramManagerTest extends CronetTestBase {
    // URLs used for tests.
    private static final String TEST_URL = "http://127.0.0.1:8000";

    CronetTestFramework mTestFramework;

    @SmallTest
    @Feature({"Cronet"})
    public void testHistogramManager() throws Exception {
        mTestFramework = startCronetTestFramework();
        byte delta1[] = mTestFramework.mHistogramManager.getHistogramDeltas();

        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest.Builder builder = new UrlRequest.Builder(
                TEST_URL, listener, listener.getExecutor(), mTestFramework.mCronetEngine);
        builder.build().start();
        listener.blockForDone();
        byte delta2[] = mTestFramework.mHistogramManager.getHistogramDeltas();
        assertTrue(delta2.length != 0);
        assertFalse(Arrays.equals(delta1, delta2));
    }
}

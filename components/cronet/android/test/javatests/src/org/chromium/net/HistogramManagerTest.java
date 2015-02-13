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

    CronetTestActivity mActivity;

    @SmallTest
    @Feature({"Cronet"})
    public void testHistogramManager() throws Exception {
        mActivity = launchCronetTestApp();
        byte delta1[] = mActivity.mHistogramManager.getHistogramDeltas();
        assertEquals(0, delta1.length);

        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                TEST_URL, listener, listener.getExecutor());
        urlRequest.start();
        listener.blockForDone();
        byte delta2[] = mActivity.mHistogramManager.getHistogramDeltas();
        assertTrue(delta2.length != 0);
        assertFalse(Arrays.equals(delta1, delta2));
    }
}

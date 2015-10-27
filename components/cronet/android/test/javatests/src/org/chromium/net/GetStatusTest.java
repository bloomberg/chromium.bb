// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.os.ConditionVariable;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.net.TestUrlRequestCallback.ResponseStep;
import org.chromium.net.UrlRequest.Status;
import org.chromium.net.UrlRequest.StatusListener;

/**
 * Tests that {@link CronetUrlRequest#getStatus} works as expected.
 */
public class GetStatusTest extends CronetTestBase {
    private CronetTestFramework mTestFramework;

    private static class TestStatusListener extends StatusListener {
        boolean mOnStatusCalled = false;
        int mStatus = Integer.MAX_VALUE;
        private final ConditionVariable mBlock = new ConditionVariable();

        @Override
        public void onStatus(int status) {
            mOnStatusCalled = true;
            mStatus = status;
            mBlock.open();
        }

        public void waitUntilOnStatusCalled() {
            mBlock.block();
            mBlock.close();
        }
    }
    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestFramework = startCronetTestFramework();
        assertTrue(NativeTestServer.startNativeTestServer(getContext()));
    }

    @Override
    protected void tearDown() throws Exception {
        NativeTestServer.shutdownNativeTestServer();
        mTestFramework.mCronetEngine.shutdown();
        super.tearDown();
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testSimpleGet() throws Exception {
        String url = NativeTestServer.getEchoMethodURL();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        UrlRequest.Builder builder = new UrlRequest.Builder(
                url, callback, callback.getExecutor(), mTestFramework.mCronetEngine);
        UrlRequest urlRequest = builder.build();
        // Calling before request is started should give Status.INVALID,
        // since the native adapter is not created.
        TestStatusListener statusListener0 = new TestStatusListener();
        urlRequest.getStatus(statusListener0);
        statusListener0.waitUntilOnStatusCalled();
        assertTrue(statusListener0.mOnStatusCalled);
        assertEquals(Status.INVALID, statusListener0.mStatus);

        urlRequest.start();

        // Should receive a valid status.
        TestStatusListener statusListener1 = new TestStatusListener();
        urlRequest.getStatus(statusListener1);
        statusListener1.waitUntilOnStatusCalled();
        assertTrue(statusListener1.mOnStatusCalled);
        assertTrue(statusListener1.mStatus >= Status.IDLE);
        assertTrue(statusListener1.mStatus <= Status.READING_RESPONSE);

        callback.waitForNextStep();
        assertEquals(ResponseStep.ON_RESPONSE_STARTED, callback.mResponseStep);
        callback.startNextRead(urlRequest);

        // Should receive a valid status.
        TestStatusListener statusListener2 = new TestStatusListener();
        urlRequest.getStatus(statusListener2);
        statusListener2.waitUntilOnStatusCalled();
        assertTrue(statusListener2.mOnStatusCalled);
        assertTrue(statusListener1.mStatus >= Status.IDLE);
        assertTrue(statusListener1.mStatus <= Status.READING_RESPONSE);

        callback.waitForNextStep();
        assertEquals(ResponseStep.ON_READ_COMPLETED, callback.mResponseStep);

        callback.startNextRead(urlRequest);
        callback.blockForDone();

        // Calling after request done should give Status.INVALID, since
        // the native adapter is destroyed.
        TestStatusListener statusListener3 = new TestStatusListener();
        urlRequest.getStatus(statusListener3);
        statusListener3.waitUntilOnStatusCalled();
        assertTrue(statusListener3.mOnStatusCalled);
        assertEquals(Status.INVALID, statusListener3.mStatus);

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("GET", callback.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testInvalidLoadState() throws Exception {
        try {
            Status.convertLoadState(LoadState.WAITING_FOR_APPCACHE);
            fail();
        } catch (IllegalArgumentException e) {
            // Expected because LoadState.WAITING_FOR_APPCACHE is not mapped.
        }

        try {
            Status.convertLoadState(-1);
            fail();
        } catch (AssertionError e) {
            // Expected.
        } catch (IllegalArgumentException e) {
            // If assertions are disabled, an IllegalArgumentException should be thrown.
            assertEquals("No request status found.", e.getMessage());
        }

        try {
            Status.convertLoadState(16);
            fail();
        } catch (AssertionError e) {
            // Expected.
        } catch (IllegalArgumentException e) {
            // If assertions are disabled, an IllegalArgumentException should be thrown.
            assertEquals("No request status found.", e.getMessage());
        }
    }
}

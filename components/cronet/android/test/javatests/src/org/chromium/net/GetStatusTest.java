// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.os.ConditionVariable;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.net.TestUrlRequestListener.ResponseStep;

/**
 * Tests that {@link CronetUrlRequest#getStatus} works as expected.
 */
public class GetStatusTest extends CronetTestBase {
    private CronetTestActivity mActivity;

    private static class TestStatusListener implements StatusListener {
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
        mActivity = launchCronetTestApp();
        assertTrue(NativeTestServer.startNativeTestServer(getInstrumentation().getTargetContext()));
    }

    @Override
    protected void tearDown() throws Exception {
        NativeTestServer.shutdownNativeTestServer();
        mActivity.mUrlRequestContext.shutdown();
        super.tearDown();
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testSimpleGet() throws Exception {
        String url = NativeTestServer.getEchoMethodURL();
        TestUrlRequestListener listener = new TestUrlRequestListener();
        listener.setAutoAdvance(false);
        UrlRequest urlRequest =
                mActivity.mUrlRequestContext.createRequest(url, listener, listener.getExecutor());
        // Calling before request is started should give RequestStatus.INVALID,
        // since the native adapter is not created.
        TestStatusListener statusListener0 = new TestStatusListener();
        urlRequest.getStatus(statusListener0);
        statusListener0.waitUntilOnStatusCalled();
        assertTrue(statusListener0.mOnStatusCalled);
        assertEquals(RequestStatus.INVALID, statusListener0.mStatus);

        urlRequest.start();

        // Should receive a valid status.
        TestStatusListener statusListener1 = new TestStatusListener();
        urlRequest.getStatus(statusListener1);
        statusListener1.waitUntilOnStatusCalled();
        assertTrue(statusListener1.mOnStatusCalled);
        assertTrue(statusListener1.mStatus >= RequestStatus.IDLE);
        assertTrue(statusListener1.mStatus <= RequestStatus.READING_RESPONSE);

        listener.waitForNextStep();
        assertEquals(ResponseStep.ON_RESPONSE_STARTED, listener.mResponseStep);
        listener.startNextRead(urlRequest);

        // Should receive a valid status.
        TestStatusListener statusListener2 = new TestStatusListener();
        urlRequest.getStatus(statusListener2);
        statusListener2.waitUntilOnStatusCalled();
        assertTrue(statusListener2.mOnStatusCalled);
        assertTrue(statusListener1.mStatus >= RequestStatus.IDLE);
        assertTrue(statusListener1.mStatus <= RequestStatus.READING_RESPONSE);

        listener.waitForNextStep();
        assertEquals(ResponseStep.ON_READ_COMPLETED, listener.mResponseStep);

        listener.startNextRead(urlRequest);
        listener.blockForDone();

        // Calling after request done should give RequestStatus.INVALID, since
        // the native adapter is destroyed.
        TestStatusListener statusListener3 = new TestStatusListener();
        urlRequest.getStatus(statusListener3);
        statusListener3.waitUntilOnStatusCalled();
        assertTrue(statusListener3.mOnStatusCalled);
        assertEquals(RequestStatus.INVALID, statusListener3.mStatus);

        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertEquals("GET", listener.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testInvalidLoadState() throws Exception {
        try {
            RequestStatus.convertLoadState(LoadState.WAITING_FOR_APPCACHE);
            fail();
        } catch (IllegalArgumentException e) {
            // Expected because LoadState.WAITING_FOR_APPCACHE is not mapped.
        }

        try {
            RequestStatus.convertLoadState(-1);
            fail();
        } catch (AssertionError e) {
            // Expected.
        }

        try {
            RequestStatus.convertLoadState(16);
            fail();
        } catch (AssertionError e) {
            // Expected.
        }
    }

}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;

import java.nio.ByteBuffer;
import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadFactory;

/**
 * Test functionality of BidirectionalStream interface.
 */
public class BidirectionalStreamTest extends CronetTestBase {
    private CronetTestFramework mTestFramework;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestFramework = startCronetTestFramework();
        assertTrue(NativeTestServer.startNativeTestServer(getContext()));
        // Add url interceptors after native application context is initialized.
        MockUrlRequestJobFactory.setUp();
    }

    @Override
    protected void tearDown() throws Exception {
        NativeTestServer.shutdownNativeTestServer();
        mTestFramework.mCronetEngine.shutdown();
        super.tearDown();
    }

    private class TestBidirectionalStreamCallback extends BidirectionalStream.Callback {
        // Executor Service for Cronet callbacks.
        private final ExecutorService mExecutorService =
                Executors.newSingleThreadExecutor(new ExecutorThreadFactory());
        private Thread mExecutorThread;

        private class ExecutorThreadFactory implements ThreadFactory {
            public Thread newThread(Runnable r) {
                mExecutorThread = new Thread(r);
                return mExecutorThread;
            }
        }

        @Override
        public void onRequestHeadersSent(BidirectionalStream stream) {}

        @Override
        public void onResponseHeadersReceived(BidirectionalStream stream, UrlResponseInfo info) {}

        @Override
        public void onReadCompleted(
                BidirectionalStream stream, UrlResponseInfo info, ByteBuffer buffer) {}

        @Override
        public void onWriteCompleted(
                BidirectionalStream stream, UrlResponseInfo info, ByteBuffer buffer) {}

        @Override
        public void onResponseTrailersReceived(BidirectionalStream stream, UrlResponseInfo info,
                UrlResponseInfo.HeaderBlock trailers) {}

        @Override
        public void onSucceeded(BidirectionalStream stream, UrlResponseInfo info) {}

        @Override
        public void onFailed(
                BidirectionalStream stream, UrlResponseInfo info, CronetException error) {}

        @Override
        public void onCanceled(BidirectionalStream stream, UrlResponseInfo info) {}

        Executor getExecutor() {
            return mExecutorService;
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testBuilderChecks() throws Exception {
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        try {
            new BidirectionalStream.Builder(
                    null, callback, callback.getExecutor(), mTestFramework.mCronetEngine);
            fail("URL not null-checked");
        } catch (NullPointerException e) {
            assertEquals("URL is required.", e.getMessage());
        }
        try {
            new BidirectionalStream.Builder(NativeTestServer.getRedirectURL(), null,
                    callback.getExecutor(), mTestFramework.mCronetEngine);
            fail("Callback not null-checked");
        } catch (NullPointerException e) {
            assertEquals("Callback is required.", e.getMessage());
        }
        try {
            new BidirectionalStream.Builder(NativeTestServer.getRedirectURL(), callback, null,
                    mTestFramework.mCronetEngine);
            fail("Executor not null-checked");
        } catch (NullPointerException e) {
            assertEquals("Executor is required.", e.getMessage());
        }
        try {
            new BidirectionalStream.Builder(
                    NativeTestServer.getRedirectURL(), callback, callback.getExecutor(), null);
            fail("CronetEngine not null-checked");
        } catch (NullPointerException e) {
            assertEquals("CronetEngine is required.", e.getMessage());
        }
        // Verify successful creation doesn't throw.
        new BidirectionalStream.Builder(NativeTestServer.getRedirectURL(), callback,
                callback.getExecutor(), mTestFramework.mCronetEngine);
    }
}

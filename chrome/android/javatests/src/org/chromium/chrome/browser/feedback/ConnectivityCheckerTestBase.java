// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.os.Handler;
import android.os.HandlerThread;

import org.chromium.content.browser.test.NativeLibraryTestBase;
import org.chromium.net.test.BaseHttpTestServer;

import java.io.IOException;
import java.net.Socket;

/**
 * Base class for tests related to checking connectivity.
 *
 * It includes a {@link ConnectivityTestServer} which is set up and torn down automatically
 * for tests.
 */
// TODO(nyquist): fix deprecation warnings crbug.com/537053
@SuppressWarnings("deprecation")
public class ConnectivityCheckerTestBase extends NativeLibraryTestBase {
    static final int TIMEOUT_MS = 5000;
    /**
     * Port number which spells out DUMMY on a numeric keypad.
     */
    private static final int DUMMY_PORT = 38669;

    // Helper URLs for each of the given HTTP response codes.
    private static final String BASE_URL = "http://127.0.0.1:" + DUMMY_PORT;
    private static final String GENERATE_200_PATH = "/generate_200";
    static final String GENERATE_200_URL = BASE_URL + GENERATE_200_PATH;
    private static final String GENERATE_204_PATH = "/generate_204";
    static final String GENERATE_204_URL = BASE_URL + GENERATE_204_PATH;
    private static final String GENERATE_204_SLOW_PATH = "/generate_slow_204";
    static final String GENERATE_204_SLOW_URL = BASE_URL + GENERATE_204_SLOW_PATH;
    private static final String GENERATE_302_PATH = "/generate_302";
    static final String GENERATE_302_URL = BASE_URL + GENERATE_302_PATH;
    private static final String GENERATE_404_PATH = "/generate_404";
    static final String GENERATE_404_URL = BASE_URL + GENERATE_404_PATH;

    private static class ConnectivityTestServer extends BaseHttpTestServer {
        /**
         * The lock object used when inspecting and manipulating {@link #mHasStarted},
         * {@link #mHasStopped}, {@link #mHandlerThread} and {@link #mHandler}.
         */
        private final Object mLock = new Object();

        /**
         * Flag for whether the server has started yet. This field must only be accessed when
         * the thread has synchronized on {@link #mLock}.
         */
        private boolean mHasStarted;

        /**
         * Flag for whether the server has stopped yet. This field must only be accessed when
         * the thread has synchronized on {@link #mLock}.
         */
        private boolean mHasStopped;

        /**
         * A {@link HandlerThread} for {@link #mHandler}. This field must only be accessed when
         * the thread has synchronized on {@link #mLock}.
         */
        private HandlerThread mHandlerThread;

        /**
         * A Handler used for posting delayed callbacks. This field must only be accessed when
         * the thread has synchronized on {@link #mLock}.
         */
        private Handler mHandler;

        /**
         * Create an HTTP test server.
         */
        public ConnectivityTestServer() throws IOException {
            super(DUMMY_PORT, TIMEOUT_MS);
        }

        @Override
        public void run() {
            synchronized (mLock) {
                if (mHasStarted) return;
                mHasStarted = true;

                mHandlerThread = new HandlerThread("ConnectivityTestServerHandler");
                mHandlerThread.start();
                mHandler = new Handler(mHandlerThread.getLooper());
            }
            super.run();
        }

        @Override
        public void stop() {
            synchronized (mLock) {
                if (!mHasStarted || mHasStopped) return;
                mHasStopped = true;

                mHandler = null;
                mHandlerThread.quit();
            }
            super.stop();
        }

        @Override
        protected boolean validateSocket(Socket sock) {
            return sock.getInetAddress().isLoopbackAddress();
        }

        @Override
        protected org.apache.http.params.HttpParams getConnectionParams() {
            org.apache.http.params.HttpParams httpParams =
                    new org.apache.http.params.BasicHttpParams();
            httpParams.setParameter(org.apache.http.params.CoreProtocolPNames.PROTOCOL_VERSION,
                    org.apache.http.HttpVersion.HTTP_1_1);
            return httpParams;
        }

        @Override
        protected void handleGet(org.apache.http.HttpRequest request, HttpResponseCallback callback)
                throws org.apache.http.HttpException {
            String requestPath = request.getRequestLine().getUri();
            if (GENERATE_204_SLOW_PATH.equals(requestPath)) {
                sendDelayedResponse(callback, requestPath);
            } else {
                sendResponse(callback, requestPath);
            }
        }

        private void sendDelayedResponse(
                final HttpResponseCallback callback, final String requestPath) {
            synchronized (mLock) {
                if (mHandler == null) throw new IllegalStateException("Handler not created.");
                mHandler.postDelayed(new Runnable() {
                    @Override
                    public void run() {
                        sendResponse(callback, requestPath);
                    }
                }, TIMEOUT_MS);
            }
        }

        private void sendResponse(HttpResponseCallback callback, String requestPath) {
            int httpStatus = getStatusCodeFromRequestPath(requestPath);
            String reason = String.valueOf(httpStatus);
            callback.onResponse(new org.apache.http.message.BasicHttpResponse(
                    org.apache.http.HttpVersion.HTTP_1_1, httpStatus, reason));
        }

        private int getStatusCodeFromRequestPath(String requestPath) {
            switch (requestPath) {
                case GENERATE_200_PATH:
                    return org.apache.http.HttpStatus.SC_OK;
                case GENERATE_204_PATH: // Intentional fall through.
                case GENERATE_204_SLOW_PATH:
                    return org.apache.http.HttpStatus.SC_NO_CONTENT;
                case GENERATE_302_PATH:
                    return org.apache.http.HttpStatus.SC_MOVED_TEMPORARILY;
                case GENERATE_404_PATH:
                    return org.apache.http.HttpStatus.SC_NOT_FOUND;
                default:
                    return org.apache.http.HttpStatus.SC_INTERNAL_SERVER_ERROR;
            }
        }
    }

    private ConnectivityTestServer mTestServer;
    private Thread mTestServerThread;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        loadNativeLibraryAndInitBrowserProcess();
        mTestServer = new ConnectivityTestServer();
        mTestServerThread = new Thread(mTestServer);
        mTestServerThread.start();
        mTestServer.waitForServerToStart();
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stop();
        mTestServerThread.join();
        super.tearDown();
    }
}

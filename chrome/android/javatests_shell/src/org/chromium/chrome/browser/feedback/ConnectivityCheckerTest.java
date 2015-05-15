// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.content.Context;
import android.test.suitebuilder.annotation.MediumTest;

import org.apache.http.HttpException;
import org.apache.http.HttpRequest;
import org.apache.http.HttpResponse;
import org.apache.http.HttpStatus;
import org.apache.http.HttpVersion;
import org.apache.http.message.BasicHttpResponse;
import org.apache.http.params.BasicHttpParams;
import org.apache.http.params.CoreProtocolPNames;
import org.apache.http.params.HttpParams;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.net.test.BaseHttpTestServer;

import java.io.IOException;
import java.net.Socket;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for {@link ConnectivityChecker}.
 */
public class ConnectivityCheckerTest extends ChromeShellTestBase {
    private static final int TIMEOUT_MS = 5000;
    /**
     * Port number which spells out DUMMY on a numeric keypad.
     */
    private static final int DUMMY_PORT = 38669;

    // Helper URLs for each of the given HTTP response codes.
    private static final String BASE_URL = "http://127.0.0.1:" + DUMMY_PORT;
    private static final String GENERATE_200_PATH = "/generate_200";
    private static final String GENERATE_200_URL = BASE_URL + GENERATE_200_PATH;
    private static final String GENERATE_204_PATH = "/generate_204";
    private static final String GENERATE_204_URL = BASE_URL + GENERATE_204_PATH;
    private static final String GENERATE_204_SLOW_PATH = "/generate_slow_204";
    private static final String GENERATE_204_SLOW_URL = BASE_URL + GENERATE_204_SLOW_PATH;
    private static final String GENERATE_302_PATH = "/generate_302";
    private static final String GENERATE_302_URL = BASE_URL + GENERATE_302_PATH;
    private static final String GENERATE_404_PATH = "/generate_404";
    private static final String GENERATE_404_URL = BASE_URL + GENERATE_404_PATH;

    private static class ConnectivityTestServer extends BaseHttpTestServer {
        /**
         * Create an HTTP test server.
         */
        public ConnectivityTestServer() throws IOException {
            super(DUMMY_PORT, TIMEOUT_MS);
        }

        @Override
        protected boolean validateSocket(Socket sock) {
            return sock.getInetAddress().isLoopbackAddress();
        }

        @Override
        protected HttpParams getConnectionParams() {
            HttpParams httpParams = new BasicHttpParams();
            httpParams.setParameter(CoreProtocolPNames.PROTOCOL_VERSION, HttpVersion.HTTP_1_1);
            return httpParams;
        }

        @Override
        protected HttpResponse handleGet(HttpRequest request) throws HttpException {
            String requestPath = request.getRequestLine().getUri();
            int httpStatus = getStatusCodeFromRequestPath(requestPath);
            String reason = String.valueOf(httpStatus);
            return new BasicHttpResponse(HttpVersion.HTTP_1_1, httpStatus, reason);
        }

        private int getStatusCodeFromRequestPath(String requestPath) {
            switch (requestPath) {
                case GENERATE_200_PATH:
                    return HttpStatus.SC_OK;
                case GENERATE_204_PATH:
                    return HttpStatus.SC_NO_CONTENT;
                case GENERATE_204_SLOW_PATH:
                    // Forcefully delay the response.
                    try {
                        Thread.sleep(TIMEOUT_MS);
                    } catch (InterruptedException e) {
                        // Intentionally ignored.
                    }
                    return HttpStatus.SC_NO_CONTENT;
                case GENERATE_302_PATH:
                    return HttpStatus.SC_MOVED_TEMPORARILY;
                case GENERATE_404_PATH:
                    return HttpStatus.SC_NOT_FOUND;
                default:
                    return HttpStatus.SC_INTERNAL_SERVER_ERROR;
            }
        }
    }

    private static class Callback implements ConnectivityChecker.ConnectivityCheckerCallback {
        private final Semaphore mSemaphore;
        private AtomicBoolean mConnected = new AtomicBoolean();

        Callback(Semaphore semaphore) {
            mSemaphore = semaphore;
        }

        @Override
        public void onResult(boolean connected) {
            mConnected.set(connected);
            mSemaphore.release();
        }

        boolean isConnected() {
            return mConnected.get();
        }
    }

    @MediumTest
    public void testNoContentShouldWork() throws Exception {
        executeTest(GENERATE_204_URL, true, TIMEOUT_MS);
    }

    @MediumTest
    public void testSlowNoContentShouldNotWork() throws Exception {
        // Force quick timeout. The server will wait TIMEOUT_MS, so this triggers well before.
        executeTest(GENERATE_204_SLOW_URL, false, 100);
    }

    @MediumTest
    public void testHttpOKShouldFail() throws Exception {
        executeTest(GENERATE_200_URL, false, TIMEOUT_MS);
    }

    @MediumTest
    public void testMovedTemporarilyShouldFail() throws Exception {
        executeTest(GENERATE_302_URL, false, TIMEOUT_MS);
    }

    @MediumTest
    public void testNotFoundShouldFail() throws Exception {
        executeTest(GENERATE_404_URL, false, TIMEOUT_MS);
    }

    @MediumTest
    public void testInvalidURLShouldFail() throws Exception {
        executeTest("http:google.com:foo", false, TIMEOUT_MS);
    }

    private void executeTest(final String url, boolean expectedResult, final long timeoutMs)
            throws Exception {
        Context targetContext = getInstrumentation().getTargetContext();
        startChromeBrowserProcessSync(targetContext);
        ConnectivityTestServer testServer = new ConnectivityTestServer();
        Thread testServerThread = new Thread(testServer);
        testServerThread.start();
        testServer.waitForServerToStart();

        Semaphore semaphore = new Semaphore(0);
        final Callback callback = new Callback(semaphore);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ConnectivityChecker.checkConnectivity(
                        Profile.getLastUsedProfile(), url, timeoutMs, callback);
            }
        });

        assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        assertEquals("URL: " + url + ", got " + callback.isConnected() + ", want " + expectedResult,
                expectedResult, callback.isConnected());

        testServer.stop();
        testServerThread.join();
    }
}

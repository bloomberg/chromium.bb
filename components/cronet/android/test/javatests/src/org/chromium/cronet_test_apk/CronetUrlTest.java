// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_test_apk;

import android.os.ConditionVariable;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.net.ChromiumUrlRequest;
import org.chromium.net.HttpUrlRequest;
import org.chromium.net.HttpUrlRequestFactoryConfig;
import org.chromium.net.HttpUrlRequestListener;

import java.io.File;
import java.util.HashMap;

/**
 * Example test that just starts the cronet sample.
 */
public class CronetUrlTest extends CronetTestBase {
    // URL used for base tests.
    private static final String URL = "http://127.0.0.1:8000";

    @SmallTest
    @Feature({"Cronet"})
    public void testLoadUrl() throws Exception {
        CronetTestActivity activity = launchCronetTestAppWithUrl(URL);

        // Make sure the activity was created as expected.
        assertNotNull(activity);

        waitForActiveShellToBeDoneLoading();

        // Make sure that the URL is set as expected.
        assertEquals(URL, activity.getUrl());
        assertEquals(200, activity.getHttpStatusCode());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testInvalidUrl() throws Exception {
        CronetTestActivity activity = launchCronetTestAppWithUrl(
                "127.0.0.1:8000");

        // Make sure the activity was created as expected.
        assertNotNull(activity);

        waitForActiveShellToBeDoneLoading();

        // The load should fail.
        assertEquals(0, activity.getHttpStatusCode());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testPostData() throws Exception {
        String[] commandLineArgs = {
                CronetTestActivity.POST_DATA_KEY, "test" };
        CronetTestActivity activity =
                launchCronetTestAppWithUrlAndCommandLineArgs(URL,
                                                             commandLineArgs);

        // Make sure the activity was created as expected.
        assertNotNull(activity);

        waitForActiveShellToBeDoneLoading();

        // Make sure that the URL is set as expected.
        assertEquals(URL, activity.getUrl());
        assertEquals(200, activity.getHttpStatusCode());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testNetLog() throws Exception {
        CronetTestActivity activity = launchCronetTestAppWithUrl(
                "127.0.0.1:8000");

        // Make sure the activity was created as expected.
        assertNotNull(activity);

        waitForActiveShellToBeDoneLoading();
        File file = File.createTempFile("cronet", "json");
        activity.mChromiumRequestFactory.getRequestContext().startNetLogToFile(
                file.getPath());
        activity.startWithURL(URL);
        Thread.sleep(5000);
        activity.mChromiumRequestFactory.getRequestContext().stopNetLog();
        assertTrue(file.exists());
        assertTrue(file.length() != 0);
        assertTrue(file.delete());
        assertTrue(!file.exists());
    }

    class SimpleHttpUrlRequestListener implements HttpUrlRequestListener {
        ConditionVariable mComplete = new ConditionVariable();
        public String negotiatedProtocol;
        public int httpStatusCode = 0;
        public byte httpResponseData[];

        public SimpleHttpUrlRequestListener() {
        }

        @Override
        public void onResponseStarted(HttpUrlRequest request) {
            negotiatedProtocol = request.getNegotiatedProtocol();
            httpStatusCode = request.getHttpStatusCode();
        }

        @Override
        public void onRequestComplete(HttpUrlRequest request) {
            httpResponseData = request.getResponseAsBytes();
            mComplete.open();
        }

        public void blockForComplete() {
            mComplete.block();
        }

        public void resetComplete() {
            mComplete.close();
        }
    }

    class BadHttpUrlRequestListener extends SimpleHttpUrlRequestListener {
        static final String THROW_TAG = "BadListener";

        public BadHttpUrlRequestListener() {
        }

        @Override
        public void onResponseStarted(HttpUrlRequest request) {
            throw new NullPointerException(THROW_TAG);
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testCalledByNativeException() throws Exception {
        CronetTestActivity activity = launchCronetTestAppWithUrl(URL);

        // Make sure the activity was created as expected.
        assertNotNull(activity);

        waitForActiveShellToBeDoneLoading();

        HashMap<String, String> headers = new HashMap<String, String>();
        BadHttpUrlRequestListener listener = new BadHttpUrlRequestListener();

        // Create request with bad listener to trigger an exception.
        HttpUrlRequest request = activity.mChromiumRequestFactory.createRequest(
                URL, HttpUrlRequest.REQUEST_PRIORITY_MEDIUM, headers, listener);
        request.start();
        listener.blockForComplete();
        assertTrue(request.isCanceled());
        assertNotNull(request.getException());
        assertEquals(listener.THROW_TAG,
                     request.getException().getCause().getMessage());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testSetUploadDataWithNullContentType() throws Exception {
        CronetTestActivity activity = launchCronetTestAppWithUrl(URL);

        // Make sure the activity was created as expected.
        assertNotNull(activity);

        waitForActiveShellToBeDoneLoading();

        HashMap<String, String> headers = new HashMap<String, String>();
        BadHttpUrlRequestListener listener = new BadHttpUrlRequestListener();

        // Create request.
        HttpUrlRequest request = activity.mChromiumRequestFactory.createRequest(
                URL, HttpUrlRequest.REQUEST_PRIORITY_MEDIUM, headers, listener);
        byte[] uploadData = new byte[] {1, 2, 3};
        try {
            request.setUploadData(null, uploadData);
            fail("setUploadData should throw on null content type");
        } catch (NullPointerException e) {
            // Nothing to do here.
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    public void disabled_testQuicLoadUrl() throws Exception {
        HttpUrlRequestFactoryConfig config = new HttpUrlRequestFactoryConfig();
        // TODO(mef): Test Quic end-to-end using local QUIC server.
        String quicURL = "https://www.google.com:443";
        String quicNegotiatedProtocol = "quic/1+spdy/3";
        config.enableQUIC(true);
        config.addQuicHint("www.google.com", 443, 443);

        String[] commandLineArgs = {
                CronetTestActivity.CONFIG_KEY, config.toString() };
        CronetTestActivity activity =
                launchCronetTestAppWithUrlAndCommandLineArgs(quicURL,
                                                             commandLineArgs);

        // Make sure the activity was created as expected.
        assertNotNull(activity);
        waitForActiveShellToBeDoneLoading();

        HashMap<String, String> headers = new HashMap<String, String>();
        SimpleHttpUrlRequestListener listener =
                new SimpleHttpUrlRequestListener();

        // Try several times as first request may not use QUIC.
        // TODO(mef): Remove loop after adding http server properties manager.
        for (int i = 0; i < 10; ++i) {
            ChromiumUrlRequest request =
                    activity.mChromiumRequestFactory.createRequest(
                            quicURL,
                            HttpUrlRequest.REQUEST_PRIORITY_MEDIUM,
                            headers,
                            listener);
            request.start();
            listener.blockForComplete();
            assertEquals(200, listener.httpStatusCode);
            if (listener.negotiatedProtocol.equals(quicNegotiatedProtocol))
                break;

            Thread.sleep(1000);
            listener.resetComplete();
        }

        assertEquals(quicNegotiatedProtocol, listener.negotiatedProtocol);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testLegacyLoadUrl() throws Exception {
        HttpUrlRequestFactoryConfig config = new HttpUrlRequestFactoryConfig();
        config.enableLegacyMode(true);
        // TODO(mef) fix tests so that library isn't loaded for legacy stack
        config.setLibraryName("cronet_tests");

        String[] commandLineArgs = {
                CronetTestActivity.CONFIG_KEY, config.toString() };
        CronetTestActivity activity =
                launchCronetTestAppWithUrlAndCommandLineArgs(URL,
                                                             commandLineArgs);

        // Make sure the activity was created as expected.
        assertNotNull(activity);

        waitForActiveShellToBeDoneLoading();

        // Make sure that the URL is set as expected.
        assertEquals(URL, activity.getUrl());
        assertEquals(200, activity.getHttpStatusCode());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testRequestHead() throws Exception {
        CronetTestActivity activity = launchCronetTestAppWithUrl(URL);

        // Make sure the activity was created as expected.
        waitForActiveShellToBeDoneLoading();

        HashMap<String, String> headers = new HashMap<String, String>();
        SimpleHttpUrlRequestListener listener =
                new SimpleHttpUrlRequestListener();

        // Create request.
        HttpUrlRequest request = activity.mChromiumRequestFactory.createRequest(
                URL, HttpUrlRequest.REQUEST_PRIORITY_MEDIUM, headers, listener);
        request.setHttpMethod("HEAD");
        request.start();
        listener.blockForComplete();
        assertEquals(200, listener.httpStatusCode);
        // HEAD requests do not get any response data and Content-Length must be
        // ignored.
        assertEquals(0, listener.httpResponseData.length);
    }
}

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.PathUtils;
import org.chromium.base.test.util.Feature;

import java.io.File;
import java.util.HashMap;

/**
 * Test for deprecated {@link HttpUrlRequest} API.
 */
public class CronetUrlTest extends CronetTestBase {
    // URL used for base tests.
    private static final String URL = "http://127.0.0.1:8000";

    @SmallTest
    @Feature({"Cronet"})
    public void testLoadUrl() throws Exception {
        CronetTestFramework testFramework = startCronetTestFrameworkWithUrl(URL);

        // Make sure that the URL is set as expected.
        assertEquals(URL, testFramework.getUrl());
        assertEquals(200, testFramework.getHttpStatusCode());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testInvalidUrl() throws Exception {
        CronetTestFramework testFramework = startCronetTestFrameworkWithUrl("127.0.0.1:8000");

        // The load should fail.
        assertEquals(0, testFramework.getHttpStatusCode());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testPostData() throws Exception {
        String[] commandLineArgs = {CronetTestFramework.POST_DATA_KEY, "test"};
        CronetTestFramework testFramework =
                startCronetTestFrameworkWithUrlAndCommandLineArgs(URL, commandLineArgs);

        // Make sure that the URL is set as expected.
        assertEquals(URL, testFramework.getUrl());
        assertEquals(200, testFramework.getHttpStatusCode());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testNetLog() throws Exception {
        Context context = getContext();
        File directory = new File(PathUtils.getDataDirectory(context));
        File file = File.createTempFile("cronet", "json", directory);
        HttpUrlRequestFactory factory = HttpUrlRequestFactory.createFactory(
                context,
                new UrlRequestContextConfig().setLibraryName("cronet_tests"));
        // Start NetLog immediately after the request context is created to make
        // sure that the call won't crash the app even when the native request
        // context is not fully initialized. See crbug.com/470196.
        factory.startNetLogToFile(file.getPath(), false);
        // Starts a request.
        HashMap<String, String> headers = new HashMap<String, String>();
        TestHttpUrlRequestListener listener = new TestHttpUrlRequestListener();
        HttpUrlRequest request = factory.createRequest(
                URL, HttpUrlRequest.REQUEST_PRIORITY_MEDIUM, headers, listener);
        request.start();
        listener.blockForComplete();
        factory.stopNetLog();
        assertTrue(file.exists());
        assertTrue(file.length() != 0);
        assertTrue(file.delete());
        assertTrue(!file.exists());
    }

    static class BadHttpUrlRequestListener extends TestHttpUrlRequestListener {
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
        CronetTestFramework testFramework = startCronetTestFrameworkWithUrl(URL);

        HashMap<String, String> headers = new HashMap<String, String>();
        BadHttpUrlRequestListener listener = new BadHttpUrlRequestListener();

        // Create request with bad listener to trigger an exception.
        HttpUrlRequest request = testFramework.mRequestFactory.createRequest(
                URL, HttpUrlRequest.REQUEST_PRIORITY_MEDIUM, headers, listener);
        request.start();
        listener.blockForComplete();
        assertTrue(request.isCanceled());
        assertNotNull(request.getException());
        assertEquals(BadHttpUrlRequestListener.THROW_TAG,
                     request.getException().getCause().getMessage());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testSetUploadDataWithNullContentType() throws Exception {
        CronetTestFramework testFramework = startCronetTestFrameworkWithUrl(URL);

        HashMap<String, String> headers = new HashMap<String, String>();
        BadHttpUrlRequestListener listener = new BadHttpUrlRequestListener();

        // Create request.
        HttpUrlRequest request = testFramework.mRequestFactory.createRequest(
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
    public void testLegacyLoadUrl() throws Exception {
        HttpUrlRequestFactoryConfig config = new HttpUrlRequestFactoryConfig();
        config.enableLegacyMode(true);
        // TODO(mef) fix tests so that library isn't loaded for legacy stack
        config.setLibraryName("cronet_tests");

        String[] commandLineArgs = {CronetTestFramework.CONFIG_KEY, config.toString()};
        CronetTestFramework testFramework =
                startCronetTestFrameworkWithUrlAndCommandLineArgs(URL, commandLineArgs);

        // Make sure that the URL is set as expected.
        assertEquals(URL, testFramework.getUrl());
        assertEquals(200, testFramework.getHttpStatusCode());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testRequestHead() throws Exception {
        CronetTestFramework testFramework = startCronetTestFrameworkWithUrl(URL);

        HashMap<String, String> headers = new HashMap<String, String>();
        TestHttpUrlRequestListener listener = new TestHttpUrlRequestListener();

        // Create request.
        HttpUrlRequest request = testFramework.mRequestFactory.createRequest(
                URL, HttpUrlRequest.REQUEST_PRIORITY_MEDIUM, headers, listener);
        request.setHttpMethod("HEAD");
        request.start();
        listener.blockForComplete();
        assertEquals(200, listener.mHttpStatusCode);
        // HEAD requests do not get any response data and Content-Length must be
        // ignored.
        assertEquals(0, listener.mResponseAsBytes.length);
    }
}

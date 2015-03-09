// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.ContextWrapper;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;

import java.util.HashMap;

/**
 * Tests that make sure ChromiumUrlRequestContext initialization will not
 * affect embedders' ability to make requests.
 */
public class ContextInitTest extends CronetTestBase {
    // URL used for base tests.
    private static final String URL = "http://127.0.0.1:8000";
    // URL used for tests that return HTTP not found (404).
    private static final String URL_404 = "http://127.0.0.1:8000/notfound404";

    @SmallTest
    @Feature({"Cronet"})
    public void testInitFactoryAndStartRequest() {
        CronetTestActivity activity = skipFactoryInitInOnCreate();

        // Immediately make a request after initializing the factory.
        HttpUrlRequestFactory factory = activity.initRequestFactory();
        TestHttpUrlRequestListener listener = makeRequest(factory, URL);
        listener.blockForComplete();
        assertEquals(200, listener.mHttpStatusCode);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testInitFactoryStartRequestAndCancel() {
        CronetTestActivity activity = skipFactoryInitInOnCreate();

        // Make a request and cancel it after initializing the factory.
        HttpUrlRequestFactory factory = activity.initRequestFactory();
        HashMap<String, String> headers = new HashMap<String, String>();
        TestHttpUrlRequestListener listener = new TestHttpUrlRequestListener();
        HttpUrlRequest request = factory.createRequest(
                URL, HttpUrlRequest.REQUEST_PRIORITY_MEDIUM, headers, listener);
        request.start();
        request.cancel();
        listener.blockForComplete();
        assertEquals(0, listener.mHttpStatusCode);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testInitFactoryStartTwoRequests() throws Exception {
        CronetTestActivity activity = skipFactoryInitInOnCreate();

        // Make two request right after initializing the factory.
        int[] statusCodes = {0, 0};
        String[] urls = {URL, URL_404};
        HttpUrlRequestFactory factory = activity.initRequestFactory();
        for (int i = 0; i < 2; i++) {
            TestHttpUrlRequestListener listener = makeRequest(factory, urls[i]);
            listener.blockForComplete();
            statusCodes[i] = listener.mHttpStatusCode;
        }
        assertEquals(200, statusCodes[0]);
        assertEquals(404, statusCodes[1]);
    }

    class RequestThread extends Thread {
        public TestHttpUrlRequestListener mListener;

        final CronetTestActivity mActivity;
        final String mUrl;

        public RequestThread(CronetTestActivity activity, String url) {
            mActivity = activity;
            mUrl = url;
        }

        @Override
        public void run() {
            HttpUrlRequestFactory factory = mActivity.initRequestFactory();
            mListener = makeRequest(factory, mUrl);
            mListener.blockForComplete();
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testInitTwoFactoriesSimultaneously() throws Exception {
        final CronetTestActivity activity = skipFactoryInitInOnCreate();

        RequestThread thread1 = new RequestThread(activity, URL);
        RequestThread thread2 = new RequestThread(activity, URL_404);

        thread1.start();
        thread2.start();
        thread1.join();
        thread2.join();
        assertEquals(200, thread1.mListener.mHttpStatusCode);
        assertEquals(404, thread2.mListener.mHttpStatusCode);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testInitTwoFactoriesInSequence() throws Exception {
        final CronetTestActivity activity = skipFactoryInitInOnCreate();

        RequestThread thread1 = new RequestThread(activity, URL);
        RequestThread thread2 = new RequestThread(activity, URL_404);

        thread1.start();
        thread1.join();
        thread2.start();
        thread2.join();
        assertEquals(200, thread1.mListener.mHttpStatusCode);
        assertEquals(404, thread2.mListener.mHttpStatusCode);
    }

    // Helper function to make a request.
    private TestHttpUrlRequestListener makeRequest(
            HttpUrlRequestFactory factory, String url) {
        HashMap<String, String> headers = new HashMap<String, String>();
        TestHttpUrlRequestListener listener = new TestHttpUrlRequestListener();
        HttpUrlRequest request = factory.createRequest(
                url, HttpUrlRequest.REQUEST_PRIORITY_MEDIUM, headers, listener);
        request.start();
        return listener;
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testInitDifferentContexts() throws Exception {
        // Test that concurrently instantiating ChromiumUrlRequestContext's upon
        // various different versions of the same Android Context does not cause
        // crashes like crbug.com/453845
        final CronetTestActivity activity = launchCronetTestApp();
        HttpUrlRequestFactory firstFactory =
                HttpUrlRequestFactory.createFactory(activity, activity.getContextConfig());
        HttpUrlRequestFactory secondFactory = HttpUrlRequestFactory.createFactory(
                activity.getApplicationContext(), activity.getContextConfig());
        HttpUrlRequestFactory thirdFactory = HttpUrlRequestFactory.createFactory(
                new ContextWrapper(activity), activity.getContextConfig());
        // Meager attempt to extend lifetimes to ensure they're concurrently
        // alive.
        firstFactory.getName();
        secondFactory.getName();
        thirdFactory.getName();
    }
}

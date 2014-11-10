// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_test_apk;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.net.HttpUrlRequest;
import org.chromium.net.HttpUrlRequestFactory;

import java.util.HashMap;

/**
 * Tests that make sure ChromiumUrlRequestContext initialization will not
 * affect embedders' ability to make requests.
 */
public class ContextInitTest extends CronetTestBase {
    // URL used for base tests.
    private static final String URL = "http://127.0.0.1:8000";

    @SmallTest
    @Feature({"Cronet"})
    public void testInitFactoryAndStartRequest() {
        CronetTestActivity activity = skipFactoryInitInOnCreate();
        // Make sure the activity was created as expected.
        assertNotNull(activity);
        // Make sure the factory is not created.
        assertNull(activity.mRequestFactory);

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
        // Make sure the activity was created as expected.
        assertNotNull(activity);
        // Make sure the factory is not created.
        assertNull(activity.mRequestFactory);

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
        // Make sure the activity was created as expected.
        assertNotNull(activity);
        // Make sure the factory is not created.
        assertNull(activity.mRequestFactory);

        // Make two request right after initializing the factory.
        int[] statusCodes = {0, 0};
        String[] urls = {URL, "http://127.0.0.1:8000/test"};
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
        // Make sure the activity was created as expected.
        assertNotNull(activity);
        // Make sure the factory is not created.
        assertNull(activity.mRequestFactory);

        RequestThread thread1 = new RequestThread(activity, URL);
        RequestThread thread2 = new RequestThread(activity,
                "http://127.0.0.1:8000/notfound");

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
        // Make sure the activity was created as expected.
        assertNotNull(activity);
        // Make sure the factory is not created.
        assertNull(activity.mRequestFactory);

        RequestThread thread1 = new RequestThread(activity, URL);
        RequestThread thread2 = new RequestThread(activity,
                "http://127.0.0.1:8000/notfound");

        thread1.start();
        thread1.join();
        thread2.start();
        thread2.join();
        assertEquals(200, thread1.mListener.mHttpStatusCode);
        assertEquals(404, thread2.mListener.mHttpStatusCode);
    }

    // Helper method to tell the activity to skip factory init in onCreate().
    private CronetTestActivity skipFactoryInitInOnCreate() {
        String[] commandLineArgs = {
                CronetTestActivity.SKIP_FACTORY_INIT_KEY, "skip" };
        CronetTestActivity activity =
                launchCronetTestAppWithUrlAndCommandLineArgs(null,
                                                             commandLineArgs);
        return activity;
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
}

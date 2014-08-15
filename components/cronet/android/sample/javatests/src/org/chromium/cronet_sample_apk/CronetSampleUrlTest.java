// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_sample_apk;

import android.os.ConditionVariable;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.net.HttpUrlRequest;
import org.chromium.net.HttpUrlRequestFactoryConfig;
import org.chromium.net.HttpUrlRequestListener;

import java.io.File;
import java.util.HashMap;

/**
 * Example test that just starts the cronet sample.
 */
public class CronetSampleUrlTest extends CronetSampleTestBase {
    // URL used for base tests.
    private static final String URL = "http://127.0.0.1:8000";

    @SmallTest
    @Feature({"Cronet"})
    public void testLoadUrl() throws Exception {
        CronetSampleActivity activity = launchCronetSampleWithUrl(URL);

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
        CronetSampleActivity activity = launchCronetSampleWithUrl(
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
                CronetSampleActivity.POST_DATA_KEY, "test" };
        CronetSampleActivity activity =
                launchCronetSampleWithUrlAndCommandLineArgs(URL,
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
        CronetSampleActivity activity = launchCronetSampleWithUrl(
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

    class BadHttpUrlRequestListener implements HttpUrlRequestListener {
        static final String THROW_TAG = "BadListener";
        ConditionVariable mComplete = new ConditionVariable();

        public BadHttpUrlRequestListener() {
        }

        @Override
        public void onResponseStarted(HttpUrlRequest request) {
            throw new NullPointerException(THROW_TAG);
        }

        @Override
        public void onRequestComplete(HttpUrlRequest request) {
            mComplete.open();
            throw new NullPointerException(THROW_TAG);
        }

        public void blockForComplete() {
            mComplete.block();
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testCalledByNativeException() throws Exception {
        CronetSampleActivity activity = launchCronetSampleWithUrl(URL);

        // Make sure the activity was created as expected.
        assertNotNull(activity);

        waitForActiveShellToBeDoneLoading();

        HashMap<String, String> headers = new HashMap<String, String>();
        BadHttpUrlRequestListener listener = new BadHttpUrlRequestListener();

        // Create request with null listener to trigger an exception.
        HttpUrlRequest request = activity.mChromiumRequestFactory.createRequest(
                URL, HttpUrlRequest.REQUEST_PRIORITY_MEDIUM, headers, listener);
        request.start();
        listener.blockForComplete();
        assertTrue(request.isCanceled());
        assertNotNull(request.getException());
        assertEquals(listener.THROW_TAG, request.getException().getCause().getMessage());

    }

    @SmallTest
    @Feature({"Cronet"})
    public void testLegacyLoadUrl() throws Exception {
        HttpUrlRequestFactoryConfig config = new HttpUrlRequestFactoryConfig();
        config.enableLegacyMode(true);

        String[] commandLineArgs = {
                CronetSampleActivity.CONFIG_KEY, config.toString() };
        CronetSampleActivity activity =
                launchCronetSampleWithUrlAndCommandLineArgs(URL,
                                                            commandLineArgs);

        // Make sure the activity was created as expected.
        assertNotNull(activity);

        waitForActiveShellToBeDoneLoading();

        // Make sure that the URL is set as expected.
        assertEquals(URL, activity.getUrl());
        assertEquals(200, activity.getHttpStatusCode());
    }
}

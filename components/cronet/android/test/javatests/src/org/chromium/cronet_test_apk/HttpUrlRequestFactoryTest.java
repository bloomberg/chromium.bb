// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_test_apk;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.net.HttpUrlRequest;
import org.chromium.net.HttpUrlRequestFactory;
import org.chromium.net.HttpUrlRequestFactoryConfig;
import org.chromium.net.UrlRequestContextConfig;

import java.util.HashMap;
import java.util.regex.Pattern;

/**
 * Tests for {@link HttpUrlRequestFactory}
 */
public class HttpUrlRequestFactoryTest extends CronetTestBase {
    // URL used for base tests.
    private static final String URL = "http://127.0.0.1:8000";

    @SmallTest
    @Feature({"Cronet"})
    public void testCreateFactory() throws Throwable {
        HttpUrlRequestFactoryConfig config = new HttpUrlRequestFactoryConfig();
        config.enableQUIC(true);
        config.addQuicHint("www.google.com", 443, 443);
        config.addQuicHint("www.youtube.com", 443, 443);
        config.setLibraryName("cronet_tests");
        String[] commandLineArgs = {
                CronetTestActivity.CONFIG_KEY, config.toString() };
        CronetTestActivity activity =
                launchCronetTestAppWithUrlAndCommandLineArgs(URL,
                                                             commandLineArgs);
        HttpUrlRequestFactory factory = activity.mRequestFactory;
        assertNotNull("Factory should be created", factory);
        assertTrue("Factory should be Chromium/n.n.n.n@r but is "
                           + factory.getName(),
                   Pattern.matches("Chromium/\\d+\\.\\d+\\.\\d+\\.\\d+@\\w+",
                           factory.getName()));
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testCreateLegacyFactory() {
        HttpUrlRequestFactoryConfig config = new HttpUrlRequestFactoryConfig();
        config.enableLegacyMode(true);

        HttpUrlRequestFactory factory = HttpUrlRequestFactory.createFactory(
                getInstrumentation().getTargetContext(), config);
        assertNotNull("Factory should be created", factory);
        assertTrue("Factory should be HttpUrlConnection/n.n.n.n@r but is "
                           + factory.getName(),
                   Pattern.matches(
                           "HttpUrlConnection/\\d+\\.\\d+\\.\\d+\\.\\d+@\\w+",
                           factory.getName()));
        HashMap<String, String> headers = new HashMap<String, String>();
        TestHttpUrlRequestListener listener = new TestHttpUrlRequestListener();
        HttpUrlRequest request = factory.createRequest(
                URL, 0, headers, listener);
        request.start();
        listener.blockForComplete();
        assertEquals(200, listener.mHttpStatusCode);
        assertEquals("OK", listener.mHttpStatusText);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testCreateLegacyFactoryUsingUrlRequestContextConfig() {
        UrlRequestContextConfig config = new UrlRequestContextConfig();
        config.enableLegacyMode(true);

        HttpUrlRequestFactory factory = HttpUrlRequestFactory.createFactory(
                getInstrumentation().getTargetContext(), config);
        assertNotNull("Factory should be created", factory);
        assertTrue("Factory should be HttpUrlConnection/n.n.n.n@r but is "
                           + factory.getName(),
                   Pattern.matches(
                           "HttpUrlConnection/\\d+\\.\\d+\\.\\d+\\.\\d+@\\w+",
                           factory.getName()));
        HashMap<String, String> headers = new HashMap<String, String>();
        TestHttpUrlRequestListener listener = new TestHttpUrlRequestListener();
        HttpUrlRequest request = factory.createRequest(
                URL, 0, headers, listener);
        request.start();
        listener.blockForComplete();
        assertEquals(200, listener.mHttpStatusCode);
        assertEquals("OK", listener.mHttpStatusText);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testQuicHintHost() {
        HttpUrlRequestFactoryConfig config = new HttpUrlRequestFactoryConfig();
        config.addQuicHint("www.google.com", 443, 443);
        try {
            config.addQuicHint("https://www.google.com", 443, 443);
        } catch (IllegalArgumentException e) {
            return;
        }
        fail("IllegalArgumentException must be thrown");
    }
}

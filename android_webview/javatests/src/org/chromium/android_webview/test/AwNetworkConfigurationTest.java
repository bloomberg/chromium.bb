// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

/**
 * A test suite for WebView's network-related configuration. This tests WebView's default settings,
 * which are configured by either AwURLRequestContextGetter or NetworkContext.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AwNetworkConfigurationTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private AwTestContainerView mTestContainerView;
    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mContainerView;
    private AwContents mAwContents;

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testSHA1LocalAnchorsAllowed() throws Throwable {
        mTestServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_SHA1_LEAF);
        try {
            CallbackHelper onReceivedSslErrorHelper = mContentsClient.getOnReceivedSslErrorHelper();
            int count = onReceivedSslErrorHelper.getCallCount();
            String url = mTestServer.getURL("/android_webview/test/data/hello_world.html");
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            // TODO(ntfschr): update this assertion whenever
            // https://android.googlesource.com/platform/external/conscrypt/+/1d6a0b8453054b7dd703693f2ce2896ae061aee3
            // rolls into an Android release, as this will mean Android intends to distrust SHA1
            // (http://crbug.com/919749).
            Assert.assertEquals("We should not have received any SSL errors", count,
                    onReceivedSslErrorHelper.getCallCount());
        } finally {
            mTestServer.stopAndDestroyServer();
        }
    }
}

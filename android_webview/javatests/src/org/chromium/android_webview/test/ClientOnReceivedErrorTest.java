// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.MediumTest;
import android.webkit.WebSettings;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.ErrorCodeConversionHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;

import java.util.concurrent.TimeUnit;

/**
 * Tests for the ContentViewClient.onReceivedError() method.
 */
public class ClientOnReceivedErrorTest extends AwTestBase {

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnReceivedErrorOnInvalidUrl() throws Throwable {
        TestCallbackHelperContainer.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();

        String url = "http://man.id.be.really.surprised.if.this.address.existed.blah/";
        int onReceivedErrorCallCount = onReceivedErrorHelper.getCallCount();
        loadUrlAsync(mAwContents, url);

        onReceivedErrorHelper.waitForCallback(onReceivedErrorCallCount,
                                              1 /* numberOfCallsToWaitFor */,
                                              WAIT_TIMEOUT_MS,
                                              TimeUnit.MILLISECONDS);
        assertEquals(ErrorCodeConversionHelper.ERROR_HOST_LOOKUP,
                onReceivedErrorHelper.getErrorCode());
        assertEquals(url, onReceivedErrorHelper.getFailingUrl());
        assertNotNull(onReceivedErrorHelper.getDescription());
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnReceivedErrorOnInvalidScheme() throws Throwable {
        TestCallbackHelperContainer.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();

        String url = "foo://some/resource";
        int onReceivedErrorCallCount = onReceivedErrorHelper.getCallCount();
        loadUrlAsync(mAwContents, url);

        onReceivedErrorHelper.waitForCallback(onReceivedErrorCallCount);
        assertEquals(ErrorCodeConversionHelper.ERROR_UNSUPPORTED_SCHEME,
                onReceivedErrorHelper.getErrorCode());
        assertEquals(url, onReceivedErrorHelper.getFailingUrl());
        assertNotNull(onReceivedErrorHelper.getDescription());
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNoErrorOnFailedSubresourceLoad() throws Throwable {
        TestCallbackHelperContainer.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

        int currentCallCount = onPageFinishedHelper.getCallCount();
        loadDataAsync(mAwContents,
                      "<html><iframe src=\"http//invalid.url.co/\" /></html>",
                      "text/html",
                      false);

        onPageFinishedHelper.waitForCallback(currentCallCount);
        assertEquals(0, onReceivedErrorHelper.getCallCount());
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNonExistentAssetUrl() throws Throwable {
        TestCallbackHelperContainer.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();
        final String url = "file:///android_asset/does_not_exist.html";
        int onReceivedErrorCallCount = onReceivedErrorHelper.getCallCount();
        loadUrlAsync(mAwContents, url);

        onReceivedErrorHelper.waitForCallback(onReceivedErrorCallCount);
        assertEquals(ErrorCodeConversionHelper.ERROR_UNKNOWN,
                     onReceivedErrorHelper.getErrorCode());
        assertEquals(url, onReceivedErrorHelper.getFailingUrl());
        assertNotNull(onReceivedErrorHelper.getDescription());
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNonExistentResourceUrl() throws Throwable {
        TestCallbackHelperContainer.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();
        final String url = "file:///android_res/raw/does_not_exist.html";
        int onReceivedErrorCallCount = onReceivedErrorHelper.getCallCount();
        loadUrlAsync(mAwContents, url);

        onReceivedErrorHelper.waitForCallback(onReceivedErrorCallCount);
        assertEquals(ErrorCodeConversionHelper.ERROR_UNKNOWN,
                     onReceivedErrorHelper.getErrorCode());
        assertEquals(url, onReceivedErrorHelper.getFailingUrl());
        assertNotNull(onReceivedErrorHelper.getDescription());
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCacheMiss() throws Throwable {
        TestCallbackHelperContainer.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();
        final String url = "http://example.com/index.html";
        int onReceivedErrorCallCount = onReceivedErrorHelper.getCallCount();
        getAwSettingsOnUiThread(mAwContents).setCacheMode(WebSettings.LOAD_CACHE_ONLY);
        loadUrlAsync(mAwContents, url);

        onReceivedErrorHelper.waitForCallback(onReceivedErrorCallCount);
        assertEquals(ErrorCodeConversionHelper.ERROR_UNKNOWN,
                     onReceivedErrorHelper.getErrorCode());
        assertEquals(url, onReceivedErrorHelper.getFailingUrl());
        assertFalse(onReceivedErrorHelper.getDescription().isEmpty());
    }
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.Build;
import android.os.Bundle;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.TextUtils;
import android.util.Pair;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.ErrorCodeConversionHelper;
import org.chromium.android_webview.policy.AwPolicyProvider;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.policy.CombinedPolicyProvider;

import java.util.ArrayList;

/** Tests for the policy based URL filtering. */
@MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT)
public class PolicyUrlFilteringTest extends AwTestBase {
    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private TestWebServer mWebServer;
    private String mFooTestUrl;
    private String mBarTestUrl;
    private AwPolicyProvider mTestProvider;

    private static final String sBlacklistPolicyName = "com.android.browser:URLBlacklist";
    private static final String sWhitelistPolicyName = "com.android.browser:URLWhitelist";

    @Override
    public void setUp() throws Exception {
        super.setUp();
        setTestAwContentsClient(new TestAwContentsClient());
        mWebServer = TestWebServer.start();
        mFooTestUrl = mWebServer.setResponse("/foo.html", "<html><body>foo</body></html>",
                new ArrayList<Pair<String, String>>());
        mBarTestUrl = mWebServer.setResponse("/bar.html", "<html><body>bar</body></html>",
                new ArrayList<Pair<String, String>>());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mTestProvider = new AwPolicyProvider(getActivity().getApplicationContext());
                CombinedPolicyProvider.get().registerProvider(mTestProvider);
            }
        });

        getInstrumentation().waitForIdleSync();
    }

    @Override
    public void tearDown() throws Exception {
        mWebServer.shutdown();
        super.tearDown();
    }

    private void setTestAwContentsClient(TestAwContentsClient contentsClient) throws Exception {
        mContentsClient = contentsClient;
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
    }

    // Tests transforming the bundle to native policies, reloading the policies and blocking
    // the navigation.
    @MediumTest
    @Feature({"AndroidWebView", "Policy"})
    public void testBlacklistedUrl() throws Throwable {
        TestCallbackHelperContainer.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

        loadUrlSync(mAwContents, onPageFinishedHelper, mFooTestUrl);
        assertEquals(mFooTestUrl, onPageFinishedHelper.getUrl());
        assertEquals(0, onReceivedErrorHelper.getCallCount());

        setFilteringPolicy(new String[] {"localhost"}, new String[] {});

        loadUrlSync(mAwContents, onPageFinishedHelper, mFooTestUrl);
        assertEquals(mFooTestUrl, onPageFinishedHelper.getUrl());
        assertEquals(1, onReceivedErrorHelper.getCallCount());
        assertEquals(ErrorCodeConversionHelper.ERROR_CONNECT, onReceivedErrorHelper.getErrorCode());
    }

    // Tests transforming the bundle to native policies, reloading the policies and getting a
    // successful navigation with a whitelist.
    @MediumTest
    @Feature({"AndroidWebView", "Policy"})
    public void testWhitelistedUrl() throws Throwable {
        TestCallbackHelperContainer.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        setFilteringPolicy(new String[] {"*"}, new String[] {mFooTestUrl});

        loadUrlSync(mAwContents, onPageFinishedHelper, mFooTestUrl);
        assertEquals(mFooTestUrl, onPageFinishedHelper.getUrl());
        assertEquals(0, onReceivedErrorHelper.getCallCount());

        // Make sure it goes through the blacklist
        loadUrlSync(mAwContents, onPageFinishedHelper, mBarTestUrl);
        assertEquals(mBarTestUrl, onPageFinishedHelper.getUrl());
        assertEquals(1, onReceivedErrorHelper.getCallCount());
        assertEquals(ErrorCodeConversionHelper.ERROR_CONNECT, onReceivedErrorHelper.getErrorCode());
    }

    // Tests that bad policy values are properly handled
    @SmallTest
    @Feature({"AndroidWebView", "Policy"})
    public void testBadPolicyValue() throws Exception {
        final Bundle newPolicies = new Bundle();
        newPolicies.putString(sBlacklistPolicyName, "shouldBeAJsonArrayNotAString");
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mTestProvider.notifySettingsAvailable(newPolicies);
            }
        });
        getInstrumentation().waitForIdleSync();

        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        loadUrlSync(mAwContents, onPageFinishedHelper, mFooTestUrl);

        // At the moment this test is written, a failure is a crash, a success is no crash.
    }

    private void setFilteringPolicy(final String[] blacklistUrls, final String[] whitelistUrls) {
        final Bundle newPolicies = new Bundle();

        if (blacklistUrls != null && blacklistUrls.length > 0) {
            String blacklistString =
                    String.format("[\"%s\"]", TextUtils.join("\",\"", blacklistUrls));
            newPolicies.putString(sBlacklistPolicyName, blacklistString);
        }

        if (whitelistUrls != null && whitelistUrls.length > 0) {
            String whitelistString =
                    String.format("[\"%s\"]", TextUtils.join("\",\"", whitelistUrls));
            newPolicies.putString(sWhitelistPolicyName, whitelistString);
        }

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mTestProvider.notifySettingsAvailable(newPolicies);
            }
        });

        // To avoid race conditions
        getInstrumentation().waitForIdleSync();
    }
}

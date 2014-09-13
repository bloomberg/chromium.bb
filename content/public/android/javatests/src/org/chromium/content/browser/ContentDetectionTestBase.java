// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.net.Uri;

import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnStartContentIntentHelper;
import org.chromium.content_shell_apk.ContentShellTestBase;

import java.util.concurrent.TimeUnit;

/**
 * Base class for content detection test suites.
 */
public class ContentDetectionTestBase extends ContentShellTestBase {

    private static final long WAIT_TIMEOUT_SECONDS = scaleTimeout(10);

    private TestCallbackHelperContainer mCallbackHelper;

    /**
     * Returns the TestCallbackHelperContainer associated with this ContentView,
     * or creates it lazily.
     */
    protected TestCallbackHelperContainer getTestCallbackHelperContainer() {
        if (mCallbackHelper == null) {
            mCallbackHelper = new TestCallbackHelperContainer(getContentViewCore());
        }
        return mCallbackHelper;
    }

    /**
     * Encodes the provided content string into an escaped url as intents do.
     * @param content Content to escape into a url.
     * @return Escaped url.
     */
    protected String urlForContent(String content) {
        return Uri.encode(content).replaceAll("%20", "+");
    }

    /**
     * Checks if the provided test url is the current url in the content view.
     * @param testUrl Test url to check.
     * @return true if the test url is the current one, false otherwise.
     */
    protected boolean isCurrentTestUrl(String testUrl) {
        return UrlUtils.getTestFileUrl(testUrl).equals(getContentViewCore()
                .getWebContents().getUrl());
    }

    /**
     * Scrolls to the node with the provided id, taps on it and waits for an intent to come.
     * @param id Id of the node to scroll and tap.
     * @return The content url of the received intent or null if none.
     */
    protected String scrollAndTapExpectingIntent(String id) throws Throwable {
        TestCallbackHelperContainer callbackHelperContainer = getTestCallbackHelperContainer();
        OnStartContentIntentHelper onStartContentIntentHelper =
                callbackHelperContainer.getOnStartContentIntentHelper();
        int currentCallCount = onStartContentIntentHelper.getCallCount();

        DOMUtils.scrollNodeIntoView(getContentViewCore(), id);
        DOMUtils.clickNode(this, getContentViewCore(), id);

        onStartContentIntentHelper.waitForCallback(currentCallCount, 1, WAIT_TIMEOUT_SECONDS,
                TimeUnit.SECONDS);
        getInstrumentation().waitForIdleSync();
        return onStartContentIntentHelper.getIntentUrl();
    }

    /**
     * Scrolls to the node with the provided id, taps on it and waits for a new page load to finish.
     * Useful when tapping on links that take to other pages.
     * @param id Id of the node to scroll and tap.
     * @return The content url of the received intent or null if none.
     */
    protected void scrollAndTapNavigatingOut(String id) throws Throwable {
        TestCallbackHelperContainer callbackHelperContainer = getTestCallbackHelperContainer();
        OnPageFinishedHelper onPageFinishedHelper =
                callbackHelperContainer.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();

        DOMUtils.scrollNodeIntoView(getContentViewCore(), id);
        DOMUtils.clickNode(this, getContentViewCore(), id);

        onPageFinishedHelper.waitForCallback(currentCallCount, 1, WAIT_TIMEOUT_SECONDS,
                TimeUnit.SECONDS);
        getInstrumentation().waitForIdleSync();
    }
}

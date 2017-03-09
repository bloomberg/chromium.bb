// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

/**
 * ActivityTestRule for content detection test suites.
 */
public class ContentDetectionActivityTestRule extends ContentShellActivityTestRule {
    private static final long WAIT_TIMEOUT_SECONDS = scaleTimeout(10);

    private final ContentDetectionTestCommon mTestCommon = new ContentDetectionTestCommon(this);

    /**
     * Returns the TestCallbackHelperContainer associated with this ContentView,
     * or creates it lazily.
     */
    public TestCallbackHelperContainer getTestCallbackHelperContainer() {
        return mTestCommon.getTestCallbackHelperContainer();
    }

    @Override
    protected void beforeActivityLaunched() {
        super.beforeActivityLaunched();
        mTestCommon.createTestContentIntentHandler();
    }

    @Override
    protected void afterActivityLaunched() {
        super.afterActivityLaunched();
        mTestCommon.setContentHandler();
    }

    /**
     * Encodes the provided content string into an escaped url as intents do.
     * @param content Content to escape into a url.
     * @return Escaped url.
     */
    public static String urlForContent(String content) {
        return ContentDetectionTestCommon.urlForContent(content);
    }

    /**
     * Checks if the provided test url is the current url in the content view.
     * @param testUrl Test url to check.
     * @return true if the test url is the current one, false otherwise.
     */
    public boolean isCurrentTestUrl(String testUrl) {
        return mTestCommon.isCurrentTestUrl(testUrl);
    }

    /**
     * Scrolls to the node with the provided id, taps on it and waits for an intent to come.
     * @param id Id of the node to scroll and tap.
     * @return The content url of the received intent or null if none.
     */
    public String scrollAndTapExpectingIntent(String id) throws Throwable {
        return mTestCommon.scrollAndTapExpectingIntent(id);
    }

    /**
     * Scrolls to the node with the provided id, taps on it and waits for a new page load to finish.
     * Useful when tapping on links that take to other pages.
     * @param id Id of the node to scroll and tap.
     */
    public void scrollAndTapNavigatingOut(String id) throws Throwable {
        mTestCommon.scrollAndTapNavigatingOut(id);
    }
}

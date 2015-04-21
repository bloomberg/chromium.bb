// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.shell.ChromeShellActivity;
import org.chromium.chrome.shell.ChromeShellTab;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.components.navigation_interception.InterceptNavigationDelegate;
import org.chromium.components.navigation_interception.NavigationParams;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TouchCommon;

import java.util.ArrayList;

/**
 * Tests for InterceptNavigationDelegate
 */
public class InterceptNavigationDelegateTest extends ChromeShellTestBase {
    private static final String BASE_URL = "chrome/test/data/navigation_interception/";
    private static final String NAVIGATION_FROM_TIMEOUT_PAGE =
            BASE_URL + "navigation_from_timer.html";
    private static final String NAVIGATION_FROM_USER_GESTURE_PAGE =
            BASE_URL + "navigation_from_user_gesture.html";
    private static final String NAVIGATION_FROM_XHR_CALLBACK_PAGE =
            BASE_URL + "navigation_from_xhr_callback.html";
    private static final String NAVIGATION_FROM_XHR_CALLBACK_AND_SHORT_TIMEOUT_PAGE =
            BASE_URL + "navigation_from_xhr_callback_and_short_timeout.html";
    private static final String NAVIGATION_FROM_XHR_CALLBACK_AND_LONG_TIMEOUT_PAGE =
            BASE_URL + "navigation_from_xhr_callback_and_long_timeout.html";
    private static final String NAVIGATION_FROM_IMAGE_ONLOAD_PAGE =
            BASE_URL + "navigation_from_image_onload.html";

    private static final long DEFAULT_MAX_TIME_TO_WAIT_IN_MS = 3000;
    private static final long LONG_MAX_TIME_TO_WAIT_IN_MS = 20000;

    private ChromeShellActivity mActivity;
    private ArrayList<NavigationParams> mHistory = new ArrayList<NavigationParams>();

    private TestInterceptNavigationDelegate mInterceptNavigationDelegate =
            new TestInterceptNavigationDelegate();

    class TestInterceptNavigationDelegate implements InterceptNavigationDelegate {
        @Override
        public boolean shouldIgnoreNavigation(NavigationParams navigationParams) {
            mHistory.add(navigationParams);
            return false;
        }
    }

    private void waitTillExpectedCallsComplete(final int count, long timeout) {
        boolean result = false;
        try {
            result = CriteriaHelper.pollForCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return mHistory.size() == count;
                }
            }, timeout, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        } catch (InterruptedException e) {
            fail("Failed while waiting for all calls to complete." + e);
        }
        assertTrue("Failed while waiting for all calls to complete.", result);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mActivity = launchChromeShellWithBlankPage();
        assertTrue(waitForActiveShellToBeDoneLoading());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ChromeShellTab tab = mActivity.getActiveTab();
                tab.setInterceptNavigationDelegate(mInterceptNavigationDelegate);
            }
        });
    }

    @SmallTest
    public void testNavigationFromTimer() throws InterruptedException {
        loadUrlWithSanitization(TestHttpServerClient.getUrl(NAVIGATION_FROM_TIMEOUT_PAGE));
        assertEquals(1, mHistory.size());

        waitTillExpectedCallsComplete(2, DEFAULT_MAX_TIME_TO_WAIT_IN_MS);
        assertEquals(false, mHistory.get(1).hasUserGesture);
        assertEquals(false, mHistory.get(1).hasUserGestureCarryover);
    }

    @SmallTest
    public void testNavigationFromUserGesture() throws InterruptedException {
        loadUrlWithSanitization(TestHttpServerClient.getUrl(NAVIGATION_FROM_USER_GESTURE_PAGE));
        assertEquals(1, mHistory.size());

        TouchCommon.singleClickView(mActivity.getActiveTab().getView(), 25, 25);
        waitTillExpectedCallsComplete(2, DEFAULT_MAX_TIME_TO_WAIT_IN_MS);
        assertEquals(true, mHistory.get(1).hasUserGesture);
        assertEquals(false, mHistory.get(1).hasUserGestureCarryover);
    }

    @SmallTest
    public void testNavigationFromXHRCallback() throws InterruptedException {
        loadUrlWithSanitization(TestHttpServerClient.getUrl(NAVIGATION_FROM_XHR_CALLBACK_PAGE));
        assertEquals(1, mHistory.size());

        TouchCommon.singleClickView(mActivity.getActiveTab().getView(), 25, 25);
        waitTillExpectedCallsComplete(2, DEFAULT_MAX_TIME_TO_WAIT_IN_MS);
        assertEquals(false, mHistory.get(1).hasUserGesture);
        assertEquals(true, mHistory.get(1).hasUserGestureCarryover);
    }

    @SmallTest
    public void testNavigationFromXHRCallbackAndShortTimeout() throws InterruptedException {
        loadUrlWithSanitization(
                TestHttpServerClient.getUrl(NAVIGATION_FROM_XHR_CALLBACK_AND_SHORT_TIMEOUT_PAGE));
        assertEquals(1, mHistory.size());

        TouchCommon.singleClickView(mActivity.getActiveTab().getView(), 25, 25);
        waitTillExpectedCallsComplete(2, DEFAULT_MAX_TIME_TO_WAIT_IN_MS);
        assertEquals(false, mHistory.get(1).hasUserGesture);
        assertEquals(true, mHistory.get(1).hasUserGestureCarryover);
    }

    @SmallTest
    public void testNavigationFromXHRCallbackAndLongTimeout() throws InterruptedException {
        loadUrlWithSanitization(
                TestHttpServerClient.getUrl(NAVIGATION_FROM_XHR_CALLBACK_AND_LONG_TIMEOUT_PAGE));
        assertEquals(1, mHistory.size());

        TouchCommon.singleClickView(mActivity.getActiveTab().getView(), 25, 25);
        waitTillExpectedCallsComplete(2, LONG_MAX_TIME_TO_WAIT_IN_MS);
        assertEquals(false, mHistory.get(1).hasUserGesture);
        assertEquals(false, mHistory.get(1).hasUserGestureCarryover);
    }

    @SmallTest
    public void testNavigationFromImageOnLoad() throws InterruptedException {
        loadUrlWithSanitization(TestHttpServerClient.getUrl(NAVIGATION_FROM_IMAGE_ONLOAD_PAGE));
        assertEquals(1, mHistory.size());

        TouchCommon.singleClickView(mActivity.getActiveTab().getView(), 25, 25);
        waitTillExpectedCallsComplete(2, DEFAULT_MAX_TIME_TO_WAIT_IN_MS);
        assertEquals(false, mHistory.get(1).hasUserGesture);
        assertEquals(true, mHistory.get(1).hasUserGestureCarryover);
    }
}
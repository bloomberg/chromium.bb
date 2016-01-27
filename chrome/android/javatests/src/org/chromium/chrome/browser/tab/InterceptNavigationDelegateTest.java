// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.components.navigation_interception.NavigationParams;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;

import java.util.ArrayList;
import java.util.concurrent.TimeoutException;

/**
 * Tests for InterceptNavigationDelegate
 */
public class InterceptNavigationDelegateTest extends ChromeActivityTestCaseBase<ChromeActivity> {
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

    private ChromeActivity mActivity;
    private ArrayList<NavigationParams> mHistory = new ArrayList<NavigationParams>();

    private TestInterceptNavigationDelegate mInterceptNavigationDelegate;

    class TestInterceptNavigationDelegate extends InterceptNavigationDelegateImpl {
        TestInterceptNavigationDelegate() {
            super(mActivity.getActivityTab());
        }

        @Override
        public boolean shouldIgnoreNavigation(NavigationParams navigationParams) {
            mHistory.add(navigationParams);
            return false;
        }
    }

    public InterceptNavigationDelegateTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    private void waitTillExpectedCallsComplete(final int count, long timeout) {
        try {
            CriteriaHelper.pollForCriteria(new Criteria(
                    "Failed while waiting for all calls to complete.") {
                @Override
                public boolean isSatisfied() {
                    return mHistory.size() == count;
                }
            }, timeout, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        } catch (InterruptedException e) {
            fail("Failed while waiting for all calls to complete." + e);
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mActivity = getActivity();
        mInterceptNavigationDelegate = new TestInterceptNavigationDelegate();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Tab tab = mActivity.getActivityTab();
                tab.setInterceptNavigationDelegate(mInterceptNavigationDelegate);
            }
        });
    }

    @SmallTest
    public void testNavigationFromTimer() throws InterruptedException {
        loadUrl(TestHttpServerClient.getUrl(NAVIGATION_FROM_TIMEOUT_PAGE));
        assertEquals(1, mHistory.size());

        waitTillExpectedCallsComplete(2, DEFAULT_MAX_TIME_TO_WAIT_IN_MS);
        assertEquals(false, mHistory.get(1).hasUserGesture);
        assertEquals(false, mHistory.get(1).hasUserGestureCarryover);
    }

    @SmallTest
    public void testNavigationFromUserGesture() throws InterruptedException, TimeoutException {
        loadUrl(TestHttpServerClient.getUrl(NAVIGATION_FROM_USER_GESTURE_PAGE));
        assertEquals(1, mHistory.size());

        DOMUtils.clickNode(this, mActivity.getActivityTab().getContentViewCore(), "first");
        waitTillExpectedCallsComplete(2, DEFAULT_MAX_TIME_TO_WAIT_IN_MS);
        assertEquals(true, mHistory.get(1).hasUserGesture);
        assertEquals(false, mHistory.get(1).hasUserGestureCarryover);
    }

    @SmallTest
    public void testNavigationFromXHRCallback() throws InterruptedException, TimeoutException {
        loadUrl(TestHttpServerClient.getUrl(NAVIGATION_FROM_XHR_CALLBACK_PAGE));
        assertEquals(1, mHistory.size());

        DOMUtils.clickNode(this, mActivity.getActivityTab().getContentViewCore(), "first");
        waitTillExpectedCallsComplete(2, DEFAULT_MAX_TIME_TO_WAIT_IN_MS);
        assertEquals(false, mHistory.get(1).hasUserGesture);
        assertEquals(true, mHistory.get(1).hasUserGestureCarryover);
    }

    @SmallTest
    public void testNavigationFromXHRCallbackAndShortTimeout()
            throws InterruptedException, TimeoutException {
        loadUrl(TestHttpServerClient.getUrl(NAVIGATION_FROM_XHR_CALLBACK_AND_SHORT_TIMEOUT_PAGE));
        assertEquals(1, mHistory.size());

        DOMUtils.clickNode(this, mActivity.getActivityTab().getContentViewCore(), "first");
        waitTillExpectedCallsComplete(2, DEFAULT_MAX_TIME_TO_WAIT_IN_MS);
        assertEquals(false, mHistory.get(1).hasUserGesture);
        assertEquals(true, mHistory.get(1).hasUserGestureCarryover);
    }

    @SmallTest
    public void testNavigationFromXHRCallbackAndLongTimeout()
            throws InterruptedException, TimeoutException {
        loadUrl(
                TestHttpServerClient.getUrl(NAVIGATION_FROM_XHR_CALLBACK_AND_LONG_TIMEOUT_PAGE));
        assertEquals(1, mHistory.size());

        DOMUtils.clickNode(this, mActivity.getActivityTab().getContentViewCore(), "first");
        waitTillExpectedCallsComplete(2, LONG_MAX_TIME_TO_WAIT_IN_MS);
        assertEquals(false, mHistory.get(1).hasUserGesture);
        assertEquals(false, mHistory.get(1).hasUserGestureCarryover);
    }

    @SmallTest
    public void testNavigationFromImageOnLoad() throws InterruptedException, TimeoutException {
        loadUrl(TestHttpServerClient.getUrl(NAVIGATION_FROM_IMAGE_ONLOAD_PAGE));
        assertEquals(1, mHistory.size());

        DOMUtils.clickNode(this, mActivity.getActivityTab().getContentViewCore(), "first");
        waitTillExpectedCallsComplete(2, DEFAULT_MAX_TIME_TO_WAIT_IN_MS);
        assertEquals(false, mHistory.get(1).hasUserGesture);
        assertEquals(true, mHistory.get(1).hasUserGestureCarryover);
    }
}

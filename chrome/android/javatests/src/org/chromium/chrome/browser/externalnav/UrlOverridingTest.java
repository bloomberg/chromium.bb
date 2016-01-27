// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalnav;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.Uri;
import android.os.SystemClock;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.TextUtils;

import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Test suite for verifying the behavior of various URL overriding actions.
 */
public class UrlOverridingTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String BASE_URL = "chrome/test/data/android/url_overriding/";
    private static final String NAVIGATION_FROM_TIMEOUT_PAGE =
            BASE_URL + "navigation_from_timer.html";
    private static final String NAVIGATION_FROM_TIMEOUT_PARENT_FRAME_PAGE =
            BASE_URL + "navigation_from_timer_parent_frame.html";
    private static final String NAVIGATION_FROM_USER_GESTURE_PAGE =
            BASE_URL + "navigation_from_user_gesture.html";
    private static final String NAVIGATION_FROM_USER_GESTURE_PARENT_FRAME_PAGE =
            BASE_URL + "navigation_from_user_gesture_parent_frame.html";
    private static final String NAVIGATION_FROM_XHR_CALLBACK_PAGE =
            BASE_URL + "navigation_from_xhr_callback.html";
    private static final String NAVIGATION_FROM_XHR_CALLBACK_PARENT_FRAME_PAGE =
            BASE_URL + "navigation_from_xhr_callback_parent_frame.html";
    private static final String NAVIGATION_FROM_XHR_CALLBACK_AND_SHORT_TIMEOUT_PAGE =
            BASE_URL + "navigation_from_xhr_callback_and_short_timeout.html";
    private static final String NAVIGATION_FROM_XHR_CALLBACK_AND_LONG_TIMEOUT_PAGE =
            BASE_URL + "navigation_from_xhr_callback_and_long_timeout.html";
    private static final String NAVIGATION_WITH_FALLBACK_URL_PAGE =
            BASE_URL + "navigation_with_fallback_url.html";
    private static final String NAVIGATION_WITH_FALLBACK_URL_PARENT_FRAME_PAGE =
            BASE_URL + "navigation_with_fallback_url_parent_frame.html";
    private static final String FALLBACK_LANDING_URL = BASE_URL + "hello.html";
    private static final String OPEN_WINDOW_FROM_USER_GESTURE_PAGE =
            BASE_URL + "open_window_from_user_gesture.html";
    private static final String NAVIGATION_FROM_JAVA_REDIRECTION_PAGE =
            BASE_URL + "navigation_from_java_redirection.html";

    private static class TestTabObserver extends EmptyTabObserver {
        private final CallbackHelper mFinishCallback;
        private final CallbackHelper mPageFailCallback;
        private final CallbackHelper mLoadFailCallback;

        TestTabObserver(final CallbackHelper finishCallback, final CallbackHelper pageFailCallback,
                final CallbackHelper loadFailCallback) {
            mFinishCallback = finishCallback;
            mPageFailCallback = pageFailCallback;
            mLoadFailCallback = loadFailCallback;
        }

        @Override
        public void onPageLoadFinished(Tab tab) {
            mFinishCallback.notifyCalled();
        }

        @Override
        public void onPageLoadFailed(Tab tab, int errorCode) {
            mPageFailCallback.notifyCalled();
        }

        @Override
        public void onDidFailLoad(Tab tab, boolean isProvisionalLoad, boolean isMainFrame,
                int errorCode, String description, String failingUrl) {
            mLoadFailCallback.notifyCalled();
        }

        @Override
        public void onDestroyed(Tab tab) {
            // A new tab is destroyed when loading is overridden while opening it.
            mPageFailCallback.notifyCalled();
        }
    }

    private ActivityMonitor mActivityMonitor;

    public UrlOverridingTest() {
        super(ChromeActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addCategory(Intent.CATEGORY_BROWSABLE);
        filter.addDataScheme("market");
        mActivityMonitor = getInstrumentation().addMonitor(
                filter, new Instrumentation.ActivityResult(Activity.RESULT_OK, null), true);
    }

    private void loadUrlAndWaitForIntentUrl(final String url, boolean needClick,
            boolean shouldLaunchExternalIntent, boolean isMainFrame) throws InterruptedException {
        loadUrlAndWaitForIntentUrl(url, needClick, 0, shouldLaunchExternalIntent, url, isMainFrame);
    }

    private void loadUrlAndWaitForIntentUrl(final String url, boolean needClick,
            int expectedNewTabCount, final boolean shouldLaunchExternalIntent,
            final String expectedFinalUrl, boolean isMainFrame) throws InterruptedException {
        final CallbackHelper finishCallback = new CallbackHelper();
        final CallbackHelper pageFailCallback = new CallbackHelper();
        final CallbackHelper loadFailCallback = new CallbackHelper();
        final CallbackHelper newTabCallback = new CallbackHelper();

        final Tab tab = getActivity().getActivityTab();
        final Tab[] latestTabHolder = new Tab[1];
        latestTabHolder[0] = tab;
        tab.addObserver(new TestTabObserver(finishCallback, pageFailCallback, loadFailCallback));
        if (expectedNewTabCount > 0) {
            getActivity().getTabModelSelector().addObserver(new EmptyTabModelSelectorObserver() {
                @Override
                public void onNewTabCreated(Tab newTab) {
                    newTabCallback.notifyCalled();
                    newTab.addObserver(new TestTabObserver(
                            finishCallback, pageFailCallback, loadFailCallback));
                    latestTabHolder[0] = newTab;
                }
            });
        }

        getActivity().onUserInteraction();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                tab.loadUrl(new LoadUrlParams(url, PageTransition.LINK));
            }
        });

        if (finishCallback.getCallCount() == 0) {
            try {
                finishCallback.waitForCallback(0, 1, 20, TimeUnit.SECONDS);
            } catch (TimeoutException ex) {
                fail();
                return;
            }
        }

        SystemClock.sleep(1);
        getActivity().onUserInteraction();
        if (needClick) {
            singleClickView(tab.getView());
        }

        CallbackHelper helper = isMainFrame ? pageFailCallback : loadFailCallback;
        if (helper.getCallCount() == 0) {
            try {
                helper.waitForCallback(0, 1, 20, TimeUnit.SECONDS);
            } catch (TimeoutException ex) {
                fail();
                return;
            }
        }

        assertEquals(expectedNewTabCount, newTabCallback.getCallCount());
        // For sub frames, the |loadFailCallback| run through different threads
        // from the ExternalNavigationHandler. As a result, there is no guarantee
        // when url override result would come.
        CriteriaHelper.pollForUIThreadCriteria(
                new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        // Note that we do not distinguish between OVERRIDE_WITH_CLOBBERING_TAB
                        // and NO_OVERRIDE since tab clobbering will eventually lead to NO_OVERRIDE.
                        // in the tab. Rather, we check the final URL to distinguish between
                        // fallback and normal navigation. See crbug.com/487364 for more.
                        Tab tab = latestTabHolder[0];
                        if (shouldLaunchExternalIntent
                                != (OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT
                                        == tab.getInterceptNavigationDelegate()
                                                .getLastOverrideUrlLoadingResultForTests())) {
                            return false;
                        }
                        return expectedFinalUrl == null
                                || TextUtils.equals(expectedFinalUrl, tab.getUrl());
                    }
                });
    }

    @SmallTest
    public void testNavigationFromTimer() throws InterruptedException {
        loadUrlAndWaitForIntentUrl(
                TestHttpServerClient.getUrl(NAVIGATION_FROM_TIMEOUT_PAGE), false, false, true);
    }

    @SmallTest
    public void testNavigationFromTimerInSubFrame() throws InterruptedException {
        loadUrlAndWaitForIntentUrl(
                TestHttpServerClient.getUrl(NAVIGATION_FROM_TIMEOUT_PARENT_FRAME_PAGE), false,
                false, false);
    }

    @SmallTest
    public void testNavigationFromUserGesture() throws InterruptedException {
        loadUrlAndWaitForIntentUrl(
                TestHttpServerClient.getUrl(NAVIGATION_FROM_USER_GESTURE_PAGE), true, true, true);
    }

    @SmallTest
    public void testNavigationFromUserGestureInSubFrame() throws InterruptedException {
        loadUrlAndWaitForIntentUrl(
                TestHttpServerClient.getUrl(NAVIGATION_FROM_USER_GESTURE_PARENT_FRAME_PAGE), true,
                true, false);
    }

    @SmallTest
    public void testNavigationFromXHRCallback() throws InterruptedException {
        loadUrlAndWaitForIntentUrl(
                TestHttpServerClient.getUrl(NAVIGATION_FROM_XHR_CALLBACK_PAGE), true, true, true);
    }

    @SmallTest
    public void testNavigationFromXHRCallbackInSubFrame() throws InterruptedException {
        loadUrlAndWaitForIntentUrl(
                TestHttpServerClient.getUrl(NAVIGATION_FROM_XHR_CALLBACK_PARENT_FRAME_PAGE), true,
                true, false);
    }

    @SmallTest
    public void testNavigationFromXHRCallbackAndShortTimeout() throws InterruptedException {
        loadUrlAndWaitForIntentUrl(
                TestHttpServerClient.getUrl(NAVIGATION_FROM_XHR_CALLBACK_AND_SHORT_TIMEOUT_PAGE),
                true, true, true);
    }

    @SmallTest
    public void testNavigationFromXHRCallbackAndLongTimeout() throws InterruptedException {
        loadUrlAndWaitForIntentUrl(
                TestHttpServerClient.getUrl(NAVIGATION_FROM_XHR_CALLBACK_AND_LONG_TIMEOUT_PAGE),
                true, false, true);
    }

    @SmallTest
    public void testNavigationWithFallbackURL() throws InterruptedException {
        loadUrlAndWaitForIntentUrl(TestHttpServerClient.getUrl(NAVIGATION_WITH_FALLBACK_URL_PAGE),
                true, 0, false, TestHttpServerClient.getUrl(FALLBACK_LANDING_URL), true);
    }

    @SmallTest
    public void testNavigationWithFallbackURLInSubFrame() throws InterruptedException {
        // Fallback URL from a subframe will not trigger main or sub frame navigation.
        loadUrlAndWaitForIntentUrl(
                TestHttpServerClient.getUrl(NAVIGATION_WITH_FALLBACK_URL_PARENT_FRAME_PAGE), true,
                false, false);
    }

    @SmallTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_TABLET)
    public void testOpenWindowFromUserGesture() throws InterruptedException {
        loadUrlAndWaitForIntentUrl(TestHttpServerClient.getUrl(OPEN_WINDOW_FROM_USER_GESTURE_PAGE),
                true, 1, true, null, true);
    }

    @SmallTest
    public void testRedirectionFromIntent() throws InterruptedException {
        Intent intent = new Intent(Intent.ACTION_VIEW,
                Uri.parse(TestHttpServerClient.getUrl(NAVIGATION_FROM_JAVA_REDIRECTION_PAGE)));
        Context targetContext = getInstrumentation().getTargetContext();
        intent.setClassName(targetContext, ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        targetContext.startActivity(intent);

        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mActivityMonitor.getHits() == 1;
            }
        });
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}

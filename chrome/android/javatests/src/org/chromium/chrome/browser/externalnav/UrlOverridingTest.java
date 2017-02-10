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
import android.support.test.filters.SmallTest;
import android.text.TextUtils;
import android.util.Base64;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.PageTransition;

import java.io.UnsupportedEncodingException;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Test suite for verifying the behavior of various URL overriding actions.
 */
public class UrlOverridingTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String BASE_PATH = "/chrome/test/data/android/url_overriding/";
    private static final String NAVIGATION_FROM_TIMEOUT_PAGE =
            BASE_PATH + "navigation_from_timer.html";
    private static final String NAVIGATION_FROM_TIMEOUT_PARENT_FRAME_PAGE =
            BASE_PATH + "navigation_from_timer_parent_frame.html";
    private static final String NAVIGATION_FROM_USER_GESTURE_PAGE =
            BASE_PATH + "navigation_from_user_gesture.html";
    private static final String NAVIGATION_FROM_USER_GESTURE_PARENT_FRAME_PAGE =
            BASE_PATH + "navigation_from_user_gesture_parent_frame.html";
    private static final String NAVIGATION_FROM_XHR_CALLBACK_PAGE =
            BASE_PATH + "navigation_from_xhr_callback.html";
    private static final String NAVIGATION_FROM_XHR_CALLBACK_PARENT_FRAME_PAGE =
            BASE_PATH + "navigation_from_xhr_callback_parent_frame.html";
    private static final String NAVIGATION_FROM_XHR_CALLBACK_AND_SHORT_TIMEOUT_PAGE =
            BASE_PATH + "navigation_from_xhr_callback_and_short_timeout.html";
    private static final String NAVIGATION_FROM_XHR_CALLBACK_AND_LONG_TIMEOUT_PAGE =
            BASE_PATH + "navigation_from_xhr_callback_and_long_timeout.html";
    private static final String NAVIGATION_WITH_FALLBACK_URL_PAGE =
            BASE_PATH + "navigation_with_fallback_url.html";
    private static final String NAVIGATION_WITH_FALLBACK_URL_PARENT_FRAME_PAGE =
            BASE_PATH + "navigation_with_fallback_url_parent_frame.html";
    private static final String FALLBACK_LANDING_PATH = BASE_PATH + "hello.html";
    private static final String OPEN_WINDOW_FROM_USER_GESTURE_PAGE =
            BASE_PATH + "open_window_from_user_gesture.html";
    private static final String NAVIGATION_FROM_JAVA_REDIRECTION_PAGE =
            BASE_PATH + "navigation_from_java_redirection.html";

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
        public void onDidFinishNavigation(Tab tab, String url, boolean isInMainFrame,
                boolean isErrorPage, boolean hasCommitted, boolean isSamePage,
                boolean isFragmentNavigation, Integer pageTransition, int errorCode,
                int httpStatusCode) {
            if (errorCode != 0) {
                mLoadFailCallback.notifyCalled();
            }
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
        public void onDidFailLoad(Tab tab, boolean isMainFrame, int errorCode, String description,
                String failingUrl) {
            mLoadFailCallback.notifyCalled();
        }

        @Override
        public void onDestroyed(Tab tab) {
            // A new tab is destroyed when loading is overridden while opening it.
            mPageFailCallback.notifyCalled();
        }
    }

    private ActivityMonitor mActivityMonitor;
    private EmbeddedTestServer mTestServer;

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
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
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
        CriteriaHelper.pollUiThread(
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
                        updateFailureReason("Expected: " + expectedFinalUrl + " actual: "
                                + tab.getUrl());
                        return expectedFinalUrl == null
                                || TextUtils.equals(expectedFinalUrl, tab.getUrl());
                    }
                });
    }

    @SmallTest
    @RetryOnFailure
    public void testNavigationFromTimer() throws InterruptedException {
        loadUrlAndWaitForIntentUrl(
                mTestServer.getURL(NAVIGATION_FROM_TIMEOUT_PAGE), false, false, true);
    }

    @SmallTest
    @RetryOnFailure
    public void testNavigationFromTimerInSubFrame() throws InterruptedException {
        loadUrlAndWaitForIntentUrl(
                mTestServer.getURL(NAVIGATION_FROM_TIMEOUT_PARENT_FRAME_PAGE), false,
                false, false);
    }

    @SmallTest
    @RetryOnFailure
    public void testNavigationFromUserGesture() throws InterruptedException {
        loadUrlAndWaitForIntentUrl(
                mTestServer.getURL(NAVIGATION_FROM_USER_GESTURE_PAGE), true, true, true);
    }

    @SmallTest
    public void testNavigationFromUserGestureInSubFrame() throws InterruptedException {
        loadUrlAndWaitForIntentUrl(
                mTestServer.getURL(NAVIGATION_FROM_USER_GESTURE_PARENT_FRAME_PAGE), true,
                true, false);
    }

    @SmallTest
    @RetryOnFailure
    public void testNavigationFromXHRCallback() throws InterruptedException {
        loadUrlAndWaitForIntentUrl(
                mTestServer.getURL(NAVIGATION_FROM_XHR_CALLBACK_PAGE), true, true, true);
    }

    @SmallTest
    @RetryOnFailure
    public void testNavigationFromXHRCallbackInSubFrame() throws InterruptedException {
        loadUrlAndWaitForIntentUrl(
                mTestServer.getURL(NAVIGATION_FROM_XHR_CALLBACK_PARENT_FRAME_PAGE), true,
                true, false);
    }

    @SmallTest
    @RetryOnFailure
    public void testNavigationFromXHRCallbackAndShortTimeout() throws InterruptedException {
        loadUrlAndWaitForIntentUrl(
                mTestServer.getURL(NAVIGATION_FROM_XHR_CALLBACK_AND_SHORT_TIMEOUT_PAGE),
                true, true, true);
    }

    @SmallTest
    @RetryOnFailure
    public void testNavigationFromXHRCallbackAndLongTimeout() throws InterruptedException {
        loadUrlAndWaitForIntentUrl(
                mTestServer.getURL(NAVIGATION_FROM_XHR_CALLBACK_AND_LONG_TIMEOUT_PAGE),
                true, false, true);
    }

    @SmallTest
    @RetryOnFailure
    public void testNavigationWithFallbackURL()
            throws InterruptedException, UnsupportedEncodingException {
        String fallbackUrl = mTestServer.getURL(FALLBACK_LANDING_PATH);
        String originalUrl = mTestServer.getURL(
                NAVIGATION_WITH_FALLBACK_URL_PAGE + "?replace_text="
                + Base64.encodeToString("PARAM_FALLBACK_URL".getBytes("utf-8"), Base64.URL_SAFE)
                + ":" + Base64.encodeToString(fallbackUrl.getBytes("utf-8"), Base64.URL_SAFE));
        loadUrlAndWaitForIntentUrl(originalUrl, true, 0, false, fallbackUrl, true);
    }

    @SmallTest
    @RetryOnFailure
    public void testNavigationWithFallbackURLInSubFrame()
            throws InterruptedException, UnsupportedEncodingException {
        // The replace_text parameters for NAVIGATION_WITH_FALLBACK_URL_PAGE, which is loaded in
        // the iframe in NAVIGATION_WITH_FALLBACK_URL_PARENT_FRAME_PAGE, have to go through the
        // embedded test server twice and, as such, have to be base64-encoded twice.
        String fallbackUrl = mTestServer.getURL(FALLBACK_LANDING_PATH);
        byte[] paramBase64Name = "PARAM_BASE64_NAME".getBytes("utf-8");
        byte[] base64ParamFallbackUrl = Base64.encode("PARAM_FALLBACK_URL".getBytes("utf-8"),
                Base64.URL_SAFE);
        byte[] paramBase64Value = "PARAM_BASE64_VALUE".getBytes("utf-8");
        byte[] base64FallbackUrl = Base64.encode(fallbackUrl.getBytes("utf-8"), Base64.URL_SAFE);

        String originalUrl = mTestServer.getURL(
                NAVIGATION_WITH_FALLBACK_URL_PARENT_FRAME_PAGE
                + "?replace_text="
                + Base64.encodeToString(paramBase64Name, Base64.URL_SAFE) + ":"
                + Base64.encodeToString(base64ParamFallbackUrl, Base64.URL_SAFE)
                + "&replace_text="
                + Base64.encodeToString(paramBase64Value, Base64.URL_SAFE) + ":"
                + Base64.encodeToString(base64FallbackUrl, Base64.URL_SAFE));

        // Fallback URL from a subframe will not trigger main or sub frame navigation.
        loadUrlAndWaitForIntentUrl(originalUrl, true, false, false);
    }

    @SmallTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_TABLET)
    public void testOpenWindowFromUserGesture() throws InterruptedException {
        loadUrlAndWaitForIntentUrl(mTestServer.getURL(OPEN_WINDOW_FROM_USER_GESTURE_PAGE),
                true, 1, true, null, true);
    }

    @SmallTest
    @RetryOnFailure
    public void testRedirectionFromIntent() {
        Intent intent = new Intent(Intent.ACTION_VIEW,
                Uri.parse(mTestServer.getURL(NAVIGATION_FROM_JAVA_REDIRECTION_PAGE)));
        Context targetContext = getInstrumentation().getTargetContext();
        intent.setClassName(targetContext, ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        targetContext.startActivity(intent);

        CriteriaHelper.pollUiThread(Criteria.equals(1, new Callable<Integer>() {
            @Override
            public Integer call() {
                return mActivityMonitor.getHits();
            }
        }));
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}

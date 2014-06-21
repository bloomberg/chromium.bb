// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.ContentViewCore.NavigationTransitionDelegate;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellTestBase;

/**
 * Test suite for navigation transition listeners.
 */
public class TransitionTest extends ContentShellTestBase {
    private static final String URL_1 = UrlUtils.encodeHtmlDataUri("<html>1</html>");

    static class TestNavigationTransitionDelegate implements NavigationTransitionDelegate {
        private boolean mDidCallDefer = false;
        private boolean mDidCallWillHandleDefer = false;
        private boolean mHandleDefer = false;
        private ContentViewCore mContentViewCore;

        TestNavigationTransitionDelegate(ContentViewCore contentViewCore, boolean handleDefer) {
            mContentViewCore = contentViewCore;
            mHandleDefer = handleDefer;
        }

        @Override
        public void didDeferAfterResponseStarted() {
            mDidCallDefer = true;
            mContentViewCore.resumeResponseDeferredAtStart();
        }

        @Override
        public boolean willHandleDeferAfterResponseStarted() {
            return mHandleDefer;
        }

        public boolean getDidCallDefer() {
            return mDidCallDefer;
        }

        public boolean getDidCallWillHandlerDefer() {
            return mDidCallWillHandleDefer;
        }
    };

    /**
     * Tests that the listener recieves DidDeferAfterResponseStarted if we specify that
     * the transition is handled.
     */
    @SmallTest
    public void testDidDeferAfterResponseStartedCalled() throws Throwable {
        ContentShellActivity activity = launchContentShellWithUrl(URL_1);
        waitForActiveShellToBeDoneLoading();
        ContentViewCore contentViewCore = activity.getActiveContentViewCore();
        TestCallbackHelperContainer testCallbackHelperContainer =
                new TestCallbackHelperContainer(contentViewCore);

        contentViewCore.setHasPendingNavigationTransitionForTesting();
        TestNavigationTransitionDelegate delegate = new TestNavigationTransitionDelegate(
                contentViewCore,
                true);
        contentViewCore.setNavigationTransitionDelegate(delegate);

        loadUrl(contentViewCore, testCallbackHelperContainer, new LoadUrlParams(URL_1));

        assertTrue("didDeferAfterResponseStarted called.", delegate.getDidCallDefer());
    }

    /**
     * Tests that the listener does not receive DidDeferAfterResponseStarted if we specify that
     * the transition is handled.
     */
    @SmallTest
    public void testDidDeferAfterResponseStartedNotCalled() throws Throwable {
        ContentShellActivity activity = launchContentShellWithUrl(URL_1);
        waitForActiveShellToBeDoneLoading();
        ContentViewCore contentViewCore = activity.getActiveContentViewCore();
        TestCallbackHelperContainer testCallbackHelperContainer =
                new TestCallbackHelperContainer(contentViewCore);

        contentViewCore.setHasPendingNavigationTransitionForTesting();
        TestNavigationTransitionDelegate delegate = new TestNavigationTransitionDelegate(
                contentViewCore,
                false);
        contentViewCore.setNavigationTransitionDelegate(delegate);

        loadUrl(contentViewCore, testCallbackHelperContainer, new LoadUrlParams(URL_1));

        assertFalse("didDeferAfterResponseStarted called.", delegate.getDidCallDefer());
    }

    /**
     * Tests that the resource handler doesn't query the listener if no transition is pending.
     */
    @SmallTest
    public void testWillHandleDeferAfterResponseStartedNotCalled() throws Throwable {
        ContentShellActivity activity = launchContentShellWithUrl(URL_1);
        waitForActiveShellToBeDoneLoading();
        ContentViewCore contentViewCore = activity.getActiveContentViewCore();
        TestCallbackHelperContainer testCallbackHelperContainer =
                new TestCallbackHelperContainer(contentViewCore);

        TestNavigationTransitionDelegate delegate = new TestNavigationTransitionDelegate(
                contentViewCore,
                false);
        contentViewCore.setNavigationTransitionDelegate(delegate);

        loadUrl(contentViewCore, testCallbackHelperContainer, new LoadUrlParams(URL_1));

        assertFalse("didDeferAfterResponseStarted called.", delegate.getDidCallDefer());
        assertFalse("willHandleDeferAfterResponseStarted called.",
                delegate.getDidCallWillHandlerDefer());
    }
}

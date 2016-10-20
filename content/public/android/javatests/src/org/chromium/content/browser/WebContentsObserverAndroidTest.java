// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellTestBase;

import java.util.concurrent.Callable;

/**
 * Tests for the WebContentsObserver APIs.
 */
public class WebContentsObserverAndroidTest extends ContentShellTestBase {
    private static final String URL = UrlUtils.encodeHtmlDataUri(
            "<html><head></head><body>didFirstVisuallyNonEmptyPaint test</body></html>");

    private static class TestWebContentsObserver extends WebContentsObserver {
        private CallbackHelper mDidFirstVisuallyNonEmptyPaintCallbackHelper = new CallbackHelper();

        public TestWebContentsObserver(WebContents webContents) {
            super(webContents);
        }

        public CallbackHelper getDidFirstVisuallyNonEmptyPaintCallbackHelper() {
            return mDidFirstVisuallyNonEmptyPaintCallbackHelper;
        }

        @Override
        public void didFirstVisuallyNonEmptyPaint() {
            mDidFirstVisuallyNonEmptyPaintCallbackHelper.notifyCalled();
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        ContentShellActivity activity = launchContentShellWithUrl(null);
        assertNotNull(activity);
        waitForActiveShellToBeDoneLoading();
    }

    /*
    @SmallTest
    @Feature({"Navigation"})
    */
    @DisabledTest(message = "crbug.com/411931")
    public void testDidFirstVisuallyNonEmptyPaint() throws Throwable {
        TestWebContentsObserver observer = ThreadUtils.runOnUiThreadBlocking(
                new Callable<TestWebContentsObserver>() {
                    @Override
                    public TestWebContentsObserver call() throws Exception {
                        return new TestWebContentsObserver(getContentViewCore().getWebContents());
                    }
                });

        int callCount = observer.getDidFirstVisuallyNonEmptyPaintCallbackHelper().getCallCount();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                getContentViewCore().getWebContents().getNavigationController()
                        .loadUrl(new LoadUrlParams(URL));
            }
        });
        observer.getDidFirstVisuallyNonEmptyPaintCallbackHelper().waitForCallback(callCount);
    }
}

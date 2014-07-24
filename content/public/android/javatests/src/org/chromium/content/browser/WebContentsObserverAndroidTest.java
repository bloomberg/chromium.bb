// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellTestBase;

import java.util.concurrent.Callable;

/**
 * Tests for the WebContentsObserverAndroid APIs.
 */
public class WebContentsObserverAndroidTest extends ContentShellTestBase {
    private static final String URL = UrlUtils.encodeHtmlDataUri(
            "<html><head></head><body>didFirstVisuallyNonEmptyPaint test</body></html>");

    private static class TestWebContentsObserverAndroid extends WebContentsObserverAndroid {
        private CallbackHelper mDidFirstVisuallyNonEmptyPaintCallbackHelper = new CallbackHelper();

        public TestWebContentsObserverAndroid(WebContents webContents) {
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

    @SmallTest
    @Feature({"Navigation"})
    public void testDidFirstVisuallyNonEmptyPaint() throws Throwable {
        TestWebContentsObserverAndroid observer = ThreadUtils.runOnUiThreadBlocking(
                new Callable<TestWebContentsObserverAndroid>() {
                    @Override
                    public TestWebContentsObserverAndroid call() throws Exception {
                        return new TestWebContentsObserverAndroid(
                                getContentViewCore().getWebContents());
                    }
                });

        int callCount = observer.getDidFirstVisuallyNonEmptyPaintCallbackHelper().getCallCount();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                getContentViewCore().loadUrl(new LoadUrlParams(URL));
            }
        });
        observer.getDidFirstVisuallyNonEmptyPaintCallbackHelper().waitForCallback(callCount);
    }
}

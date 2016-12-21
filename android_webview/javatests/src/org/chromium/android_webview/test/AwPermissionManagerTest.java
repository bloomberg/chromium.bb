// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.Handler;
import android.os.Looper;
import android.support.test.filters.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.permission.AwPermissionRequest;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.Callable;

/**
 * Test AwPermissionManager.
 */
public class AwPermissionManagerTest extends AwTestBase {

    private static final String REQUEST_DUPLICATE = "<html> <script>"
            + "navigator.requestMIDIAccess({sysex: true}).then(function() {"
            + "});"
            + "navigator.requestMIDIAccess({sysex: true}).then(function() {"
            + "  window.document.title = 'second-granted';"
            + "});"
            + "</script><body>"
            + "</body></html>";

    private TestWebServer mTestWebServer;
    private String mPage;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestWebServer = TestWebServer.start();
    }

    @Override
    protected void tearDown() throws Exception {
        mTestWebServer.shutdown();
        mTestWebServer = null;
        super.tearDown();
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testRequestMultiple() throws Throwable {
        mPage = mTestWebServer.setResponse("/permissions", REQUEST_DUPLICATE,
                CommonResources.getTextHtmlHeaders(true));

        TestAwContentsClient contentsClient = new TestAwContentsClient() {
            private boolean mCalled;

            @Override
            public void onPermissionRequest(final AwPermissionRequest awPermissionRequest) {
                if (mCalled) {
                    fail("Only one request was expected");
                    return;
                }
                mCalled = true;

                // Emulate a delayed response to the request by running four seconds in the future.
                Handler handler = new Handler(Looper.myLooper());
                handler.postDelayed(new Runnable() {
                    @Override
                    public void run() {
                        awPermissionRequest.grant();
                    }
                }, 4000);
            }
        };

        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        enableJavaScriptOnUiThread(awContents);
        loadUrlAsync(awContents, mPage, null);
        pollTitleAs("second-granted", awContents);
    }

    private void pollTitleAs(final String title, final AwContents awContents)
            throws Exception {
        pollInstrumentationThread(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return title.equals(getTitleOnUiThread(awContents));
            }
        });
    }
}


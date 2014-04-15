// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.view.KeyEvent;

import org.chromium.android_webview.test.util.VideoTestWebServer;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.TouchCommon;

/**
 * Test WebChromeClient::onShow/HideCustomView.
 */
public class AwContentsClientFullScreenVideoTest extends AwTestBase {

    /** Disabled to unblock the waterfall, investigating in http://crbug.com/361514. */
    @Feature({"AndroidWebView"})
    @DisabledTest
    public void testOnShowAndHideCustomView() throws Throwable {
        FullScreenVideoTestAwContentsClient contentsClient =
                new FullScreenVideoTestAwContentsClient(getActivity());
        AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentsClient);
        enableJavaScriptOnUiThread(testContainerView.getAwContents());
        VideoTestWebServer webServer = new VideoTestWebServer(
                getInstrumentation().getTargetContext());
        try {
            loadUrlSync(testContainerView.getAwContents(),
                    contentsClient.getOnPageFinishedHelper(),
                    webServer.getFullScreenVideoTestURL());
            Thread.sleep(5 * 1000);
            TouchCommon touchCommon = new TouchCommon(this);
            touchCommon.singleClickView(testContainerView);
            contentsClient.waitForCustomViewShown();
            getInstrumentation().sendKeyDownUpSync(KeyEvent.KEYCODE_BACK);
            contentsClient.waitForCustomViewHidden();
        } finally {
            if (webServer != null) webServer.getTestWebServer().shutdown();
        }
    }
}

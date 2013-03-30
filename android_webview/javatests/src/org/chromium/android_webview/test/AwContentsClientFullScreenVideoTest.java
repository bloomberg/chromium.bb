// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.LargeTest;
import android.test.TouchUtils;
import android.view.KeyEvent;

import org.chromium.android_webview.test.util.VideoTestWebServer;
import org.chromium.base.test.util.Feature;

/**
 * Test WebChromeClient::onShow/HideCustomView.
 */
public class AwContentsClientFullScreenVideoTest extends AwTestBase {

    @Feature({"AndroidWebView"})
    @LargeTest
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
            // Temporary code to figure out minimal wait time for page show up.
            // see crbug/224923.
            Thread.sleep(10 * 1000);
            TouchUtils.clickView(AwContentsClientFullScreenVideoTest.this, testContainerView);
            contentsClient.waitForCustomViewShown();
            getInstrumentation().sendKeyDownUpSync(KeyEvent.KEYCODE_BACK);
            contentsClient.waitForCustomViewHidden();
        }
        finally {
            if (webServer != null) webServer.getTestWebServer().shutdown();
        }
    }
}

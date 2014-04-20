// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.MediumTest;
import android.view.KeyEvent;

import org.chromium.android_webview.test.util.VideoTestWebServer;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.ContentVideoView;
import org.chromium.content.browser.test.util.TouchCommon;

/**
 * Test WebChromeClient::onShow/HideCustomView.
 */
public class AwContentsClientFullScreenVideoTest extends AwTestBase {
    private FullScreenVideoTestAwContentsClient mContentsClient;

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnShowAndHideCustomViewWithCallback() throws Throwable {
        doOnShowAndHideCustomViewTest(new Runnable() {
            @Override
            public void run() {
                mContentsClient.getExitCallback().onCustomViewHidden();
            }
        });
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnShowAndHideCustomViewWithBackKey() throws Throwable {
        doOnShowAndHideCustomViewTest(new Runnable() {
            @Override
            public void run() {
                ContentVideoView view = mContentsClient.getVideoView();
                view.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK));
                view.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_BACK));
            }
        });
    }

    private void doOnShowAndHideCustomViewTest(Runnable existFullscreen) throws Throwable {
        mContentsClient = new FullScreenVideoTestAwContentsClient(getActivity());
        AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        enableJavaScriptOnUiThread(testContainerView.getAwContents());
        VideoTestWebServer webServer = new VideoTestWebServer(
                getInstrumentation().getTargetContext());
        try {
            loadUrlSync(testContainerView.getAwContents(),
                    mContentsClient.getOnPageFinishedHelper(),
                    webServer.getFullScreenVideoTestURL());
            Thread.sleep(5 * 1000);

            TouchCommon touchCommon = new TouchCommon(this);
            touchCommon.singleClickView(testContainerView);
            mContentsClient.waitForCustomViewShown();

            getInstrumentation().runOnMainSync(existFullscreen);
            mContentsClient.waitForCustomViewHidden();
        } finally {
            if (webServer != null) webServer.getTestWebServer().shutdown();
        }
    }
}

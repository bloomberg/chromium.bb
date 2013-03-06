// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;
import android.test.TouchUtils;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.webkit.WebChromeClient;
import android.widget.FrameLayout;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.VideoTestWebServer;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;

import java.io.IOException;
import java.io.InputStream;
import java.io.ByteArrayOutputStream;

/**
 * Test for AwContentClient.GetVideoLoadingProgressView.
 *
 * This test takes advantage of onViewAttachedToWindow, and assume our progress view
 * is shown once it attached to window.
 *
 * As it needs user gesture to switch to the full screen mode video, A large button
 * used to trigger switch occupies almost the whole WebView so the simulated click event
 * can't miss it.
 */
public class AwContentsClientGetVideoLoadingProgressViewTest extends AndroidWebViewTestBase
        implements View.OnAttachStateChangeListener {
    private Object mViewAttachedEvent = new Object();
    private boolean mViewAttached = false;

    @Override
    public void onViewAttachedToWindow(View view) {
        synchronized(mViewAttachedEvent) {
            mViewAttachedEvent.notify();
            mViewAttached = true;
            view.removeOnAttachStateChangeListener(this);
        }
    }

    @Override
    public void onViewDetachedFromWindow(View arg0) {
    }

    private void waitForViewAttached() throws InterruptedException {
        synchronized(mViewAttachedEvent) {
            while (!mViewAttached) {
                mViewAttachedEvent.wait();
            }
        }
    }

    /*
    * @Feature({"AndroidWebView"})
    * @SmallTest
    * http://crbug.com/180575
    */
    @DisabledTest
    public void testGetVideoLoadingProgressView() throws Throwable {
        TestAwContentsClient contentsClient = new TestAwContentsClient() {
            @Override
            protected View getVideoLoadingProgressView() {
                View view = new View(getInstrumentation().getContext());
                view.addOnAttachStateChangeListener(
                        AwContentsClientGetVideoLoadingProgressViewTest.this);
                return view;
            }

            @Override
            public void onShowCustomView(View view, int requestedOrientation,
                    WebChromeClient.CustomViewCallback callback) {
                getActivity().getWindow().setFlags(
                        WindowManager.LayoutParams.FLAG_FULLSCREEN,
                        WindowManager.LayoutParams.FLAG_FULLSCREEN);

                getActivity().getWindow().addContentView(view,
                        new FrameLayout.LayoutParams(
                                ViewGroup.LayoutParams.MATCH_PARENT,
                                ViewGroup.LayoutParams.MATCH_PARENT,
                                Gravity.CENTER));
            }
        };
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                awContents.getContentViewCore().getContentSettings().setJavaScriptEnabled(true);
            }
        });
        VideoTestWebServer webServer = new VideoTestWebServer();
        try {
            loadDataSync(awContents, contentsClient.getOnPageFinishedHelper(),
                    getData(webServer.getOnePixelOneFrameWebmURL()), "text/html", false);
            TouchUtils.clickView(AwContentsClientGetVideoLoadingProgressViewTest.this,
                    testContainerView);
            waitForViewAttached();
        } finally {
            if (webServer != null) webServer.getTestWebServer().shutdown();
        }
    }

    private String getData(String url) throws IOException {
        InputStream in = getInstrumentation().getContext().getAssets().open(
                "get_video_loading_progress_view_test.html");
        ByteArrayOutputStream os = new ByteArrayOutputStream();
        int buflen = 128;
        byte[] buffer = new byte[buflen];
        int len = in.read(buffer, 0, buflen);
        while (len != -1) {
            os.write(buffer, 0, len);
            if (len < buflen) break;
            len = in.read(buffer, 0, buflen);
        }
        return os.toString().replace("VIDEO_FILE_URL", url);
  }
}

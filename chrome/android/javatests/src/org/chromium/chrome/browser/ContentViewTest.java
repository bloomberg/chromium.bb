// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.graphics.Rect;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Message;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.shell.ChromeShellActivity;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.test.util.CallbackHelper;

import java.util.concurrent.TimeoutException;

/**
 * Tests for the ContentView.
 */
public class ContentViewTest extends ChromeShellTestBase implements Handler.Callback {

    private static class MyCallbackHelper extends CallbackHelper {
        public String getTitle() {
            return mTitle;
        }

        public String getUrl() {
            return mUrl;
        }

        public String getText() {
            return mText;
        }

        public String getHtml() {
            return mHtml;
        }

        public Rect getRect() {
            return mRect;
        }

        public void notifyCalled(String title, String url, String text, String html, Rect rect) {
            mTitle = title;
            mUrl = url;
            mText = text;
            mHtml = html;
            mRect = rect;
            super.notifyCalled();
        }

        private String mTitle;
        private String mUrl;
        private String mText;
        private String mHtml;
        private Rect mRect;
    }

    private ChromeShellActivity mActivity;
    private MyCallbackHelper mCallbackHelper;
    private HandlerThread mHandlerThread;
    private Handler mHandler;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mActivity = launchChromeShellWithBlankPage();
        mCallbackHelper = new MyCallbackHelper();
        mHandlerThread = new HandlerThread("ContentViewTest thread");
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper(), this);
    }

    @Override
    public void tearDown() throws Exception {
        try {
            mHandlerThread.quitSafely();
        } finally {
            super.tearDown();
        }
    }

    // Implements Handler.Callback
    @Override
    public boolean handleMessage(Message msg) {
        Bundle bundle = msg.getData();
        assertNotNull(bundle);
        String url = bundle.getString("url");
        String title = bundle.getString("title");
        String text = bundle.getString("text");
        String html = bundle.getString("html");
        Rect rect = bundle.getParcelable("rect");
        // We don't care about other values for now.
        mCallbackHelper.notifyCalled(title, url, text, html, rect);
        return true;
    }

    @MediumTest
    @Feature({"SmartClip"})
    public void testSmartClipDataCallback() throws InterruptedException, TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ContentView cv = (ContentView) getActivity().getActiveTab().getView();
                assertNotNull(cv);
                cv.setSmartClipResultHandler(mHandler);
                cv.extractSmartClipData(10, 20, 100, 70);
            }
        });
        mCallbackHelper.waitForCallback(0, 1);  // call count: 0 --> 1
        assertEquals("about:blank", mCallbackHelper.getTitle());
        assertEquals("about:blank", mCallbackHelper.getUrl());
        assertNotNull(mCallbackHelper.getText());
        assertNotNull(mCallbackHelper.getHtml());
        assertNotNull(mCallbackHelper.getRect());
    }
}

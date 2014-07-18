// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Message;
import android.test.suitebuilder.annotation.MediumTest;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.shell.ChromeShellActivity;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.chrome.shell.R;
import org.chromium.content.browser.test.util.CallbackHelper;

import java.lang.reflect.Method;
import java.util.concurrent.TimeoutException;

/**
 * Tests for the SmartClipProvider.
 */
public class SmartClipProviderTest extends ChromeShellTestBase implements Handler.Callback {
    // This is a key for meta-data in the package manifest. It should NOT
    // change, as OEMs will use it when they look for the SmartClipProvider
    // interface.
    private static final String SMART_CLIP_PROVIDER_KEY =
            "org.chromium.content.browser.SMART_CLIP_PROVIDER";

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
    private Class<?> mSmartClipProviderClass;
    private Method mSetSmartClipResultHandlerMethod;
    private Method mExtractSmartClipDataMethod;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mActivity = launchChromeShellWithBlankPage();
        mCallbackHelper = new MyCallbackHelper();
        mHandlerThread = new HandlerThread("ContentViewTest thread");
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper(), this);

        mSmartClipProviderClass = getSmartClipProviderClass();
        assertNotNull(mSmartClipProviderClass);
        mSetSmartClipResultHandlerMethod = mSmartClipProviderClass.getDeclaredMethod(
                "setSmartClipResultHandler", new Class[] { Handler.class });
        mExtractSmartClipDataMethod = mSmartClipProviderClass.getDeclaredMethod(
                "extractSmartClipData",
                new Class[] { Integer.TYPE, Integer.TYPE, Integer.TYPE, Integer.TYPE });
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

    // Create SmartClipProvider interface from package meta-data.
    private Class<?> getSmartClipProviderClass() throws Exception {
        ApplicationInfo ai = mActivity.getPackageManager().getApplicationInfo(
                mActivity.getPackageName(), PackageManager.GET_META_DATA);
        Bundle bundle = ai.metaData;
        String className = bundle.getString(SMART_CLIP_PROVIDER_KEY);
        assertNotNull(className);
        return Class.forName(className);
    }

    // Returns the first smart clip provider under the root view using DFS.
    private Object findSmartClipProvider(View v) {
        if (mSmartClipProviderClass.isInstance(v)) {
            return v;
        } else if (v instanceof ViewGroup) {
            ViewGroup viewGroup = (ViewGroup) v;
            int count = viewGroup.getChildCount();
            for (int i = 0; i < count; ++i) {
                View c = viewGroup.getChildAt(i);
                Object found = findSmartClipProvider(c);
                if (found != null)
                    return found;
            }
        }
        return null;
    }

    @MediumTest
    @Feature({"SmartClip"})
    public void testSmartClipDataCallback() throws InterruptedException, TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // This emulates what OEM will be doing when they want to call
                // functions on SmartClipProvider through view hierarchy.

                // Implementation of SmartClipProvider such as ContentView or
                // JellyBeanContentView can be found somewhere under content_container.
                Object scp = findSmartClipProvider(
                        getActivity().findViewById(R.id.content_container));
                assertNotNull(scp);
                try {
                    mSetSmartClipResultHandlerMethod.invoke(scp, mHandler);
                    mExtractSmartClipDataMethod.invoke(scp, 10, 20, 100, 70);
                } catch (Exception e) {
                    e.printStackTrace();
                    fail();
                }
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

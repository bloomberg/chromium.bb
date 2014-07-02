// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.graphics.Rect;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.shell.ChromeShellActivity;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.CallbackHelper;

import java.util.concurrent.TimeoutException;

/**
 * Tests for the ContentView.
 */
public class ContentViewTest extends ChromeShellTestBase {

    private ChromeShellActivity mActivity;
    private CallbackHelper mCallbackHelper;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mActivity = launchChromeShellWithBlankPage();
        mCallbackHelper = new CallbackHelper();
    }

    @MediumTest
    @Feature({"SmartClip"})
    public void testSmartClipDataCallback() throws InterruptedException, TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ContentView cv = (ContentView) getActivity().getActiveTab().getView();
                assertNotNull(cv);
                cv.setSmartClipDataListener(new ContentViewCore.SmartClipDataListener() {
                    public void onSmartClipDataExtracted(String result, Rect cliprect) {
                        // We don't care about the returned values for now.
                        mCallbackHelper.notifyCalled();
                    }
                });
                cv.extractSmartClipData(10, 10, 100, 100);
            }
        });
        mCallbackHelper.waitForCallback(0, 1);  // call count: 0 --> 1
    }
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.KeyEvent;
import android.view.View;
import android.webkit.ConsoleMessage;
import android.widget.GridLayout;

import org.chromium.android_webview.AndroidWebViewUtil;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.base.test.Feature;
import org.chromium.content.browser.ContentViewCore;

import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/**
 * AwContents tests.
 */
public class AwContentsTest extends AndroidWebViewTestBase {
    private AwContentsClient mContentsClient = new NullContentsClient();

    private AwContents createContents(Context context) {
        GridLayout viewGroup = new GridLayout(context);

        ContentViewCore contentViewCore = new ContentViewCore(
                context, viewGroup, null, 0, ContentViewCore.PERSONALITY_VIEW);
        AwContents awContents = new AwContents(viewGroup, null, contentViewCore,
                mContentsClient, false, false);
        return awContents;
    }

    @SmallTest
    public void testCreateDestroy() throws Throwable {
        final Throwable[] error = new Throwable[1];
        final Semaphore s = new Semaphore(0);
        final Context context = getActivity();

        Runnable r = new Runnable() {
            @Override
            public void run() {
                try {
                    createContents(context).destroy();
                } catch (Throwable t) {
                    error[0] = t;
                } finally {
                    s.release();
                }
            }
        };
        new Handler(Looper.getMainLooper()).post(r);
        assertTrue(s.tryAcquire(10000, TimeUnit.MILLISECONDS));
        if (error[0] != null) {
            throw error[0];
        }
    }
}

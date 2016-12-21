// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.filters.SmallTest;

import org.chromium.android_webview.AwContentsLifecycleNotifier;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;

/**
 * AwContentsLifecycleNotifier tests.
 */
public class AwContentsLifecycleNotifierTest extends AwTestBase {

    private TestAwContentsClient mContentsClient = new TestAwContentsClient();

    private static class LifecycleObserver implements AwContentsLifecycleNotifier.Observer {
        public CallbackHelper mFirstWebViewCreatedCallback = new CallbackHelper();
        public CallbackHelper mLastWebViewDestroyedCallback = new CallbackHelper();

        @Override
        public void onFirstWebViewCreated() {
            mFirstWebViewCreatedCallback.notifyCalled();
        }

        @Override
        public void onLastWebViewDestroyed() {
            mLastWebViewDestroyedCallback.notifyCalled();
        }
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNotifierCreate() throws Throwable {
        LifecycleObserver observer = new LifecycleObserver();
        AwContentsLifecycleNotifier.addObserver(observer);
        assertFalse(AwContentsLifecycleNotifier.hasWebViewInstances());

        AwTestContainerView awTestContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        observer.mFirstWebViewCreatedCallback.waitForCallback(0, 1);
        assertTrue(AwContentsLifecycleNotifier.hasWebViewInstances());

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                getActivity().removeAllViews();
            }
        });
        destroyAwContentsOnMainSync(awTestContainerView.getAwContents());
        observer.mLastWebViewDestroyedCallback.waitForCallback(0, 1);
        assertFalse(AwContentsLifecycleNotifier.hasWebViewInstances());
    }
}

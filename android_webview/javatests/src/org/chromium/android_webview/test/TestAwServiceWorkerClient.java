// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.android_webview.AwServiceWorkerClient;
import org.chromium.android_webview.AwWebResourceResponse;
import org.chromium.base.test.util.CallbackHelper;

import java.util.ArrayList;
import java.util.List;

/**
 * AwServiceWorkerClient subclass used for testing.
 */
public class TestAwServiceWorkerClient extends AwServiceWorkerClient {

    /**
     * Helper class to wait for callbacks on the TestAwServiceWorkerClient.
     */
    public static class ShouldInterceptRequestHelper extends CallbackHelper {
        private List<AwWebResourceRequest> mInterceptedRequests =
                new ArrayList<AwWebResourceRequest>();

        public void notifyCalled(AwWebResourceRequest request) {
            mInterceptedRequests.add(request);
            notifyCalled();
        }

        public List<AwWebResourceRequest> getAwWebResourceRequests() {
            return mInterceptedRequests;
        }
    }

    private ShouldInterceptRequestHelper mShouldInterceptRequestHelper;

    public ShouldInterceptRequestHelper getShouldInterceptRequestHelper() {
        return mShouldInterceptRequestHelper;
    }

    public TestAwServiceWorkerClient() {
        mShouldInterceptRequestHelper = new ShouldInterceptRequestHelper();
    }

    @Override
    public AwWebResourceResponse shouldInterceptRequest(AwWebResourceRequest request) {
        mShouldInterceptRequestHelper.notifyCalled(request);
        return null;
    }
}

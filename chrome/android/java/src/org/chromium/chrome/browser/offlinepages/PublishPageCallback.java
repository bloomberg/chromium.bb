// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.app.Activity;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.share.ShareParams;

/**
 * This callback will save the state we need when the JNI call is done, and start the next stage of
 * processing for sharing.
 */
public class PublishPageCallback implements Callback<OfflinePageItem> {
    private Callback<ShareParams> mShareCallback;
    private Activity mActivity;
    private static final String TAG = "PublishPageCallback";

    /** Create a callback for use when page publishing is completed. */
    public PublishPageCallback(Activity activity, Callback<ShareParams> shareCallback) {
        mActivity = activity;
        mShareCallback = shareCallback;
    }

    @Override
    @CalledByNative
    /** Report results of publishing. */
    public void onResult(OfflinePageItem page) {
        OfflinePageUtils.sharePublishedPage(page, mActivity, mShareCallback);
    }
}

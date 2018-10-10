// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import com.google.android.libraries.feed.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.host.logging.ContentLoggingData;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Implementation of {@link BasicLoggingApi} that log actions performed on the Feed,
 * and provides access to native implementation of feed logging.
 */
@JNINamespace("feed")
public class FeedLoggingBridge implements BasicLoggingApi {
    private long mNativeFeedLoggingBridge;

    /**
     * Creates a {@link FeedLoggingBridge} for accessing native feed logging
     * implementation for the current user, and initial native side bridge.
     *
     * @param profile {@link Profile} of the user we are rendering the Feed for.
     */
    public FeedLoggingBridge(Profile profile) {
        mNativeFeedLoggingBridge = nativeInit(profile);
    }

    /** Cleans up native half of this bridge. */
    public void destroy() {
        assert mNativeFeedLoggingBridge != 0;
        nativeDestroy(mNativeFeedLoggingBridge);
        mNativeFeedLoggingBridge = 0;
    }

    @Override
    public void onContentViewed(ContentLoggingData data) {
        assert mNativeFeedLoggingBridge != 0;
        nativeOnContentViewed(mNativeFeedLoggingBridge, data.getPositionInStream(),
                data.getPublishedTimeSeconds(), data.getTimeContentBecameAvailable(),
                data.getScore());
    }

    @Override
    public void onContentDismissed(ContentLoggingData data) {
        assert mNativeFeedLoggingBridge != 0;
        nativeOnContentDismissed(mNativeFeedLoggingBridge, data.getRepresentationUri());
    }

    @Override
    public void onContentClicked(ContentLoggingData data) {
        assert mNativeFeedLoggingBridge != 0;
        nativeOnContentClicked(mNativeFeedLoggingBridge, data.getPositionInStream(),
                data.getPublishedTimeSeconds(), data.getScore());
    }

    @Override
    public void onClientAction(ContentLoggingData data, int actionType) {
        assert mNativeFeedLoggingBridge != 0;
        nativeOnClientAction(mNativeFeedLoggingBridge, actionType);
    }

    @Override
    public void onContentContextMenuOpened(ContentLoggingData data) {
        assert mNativeFeedLoggingBridge != 0;
        nativeOnContentContextMenuOpened(mNativeFeedLoggingBridge, data.getPositionInStream(),
                data.getPublishedTimeSeconds(), data.getScore());
    }

    @Override
    public void onMoreButtonViewed(int position) {
        assert mNativeFeedLoggingBridge != 0;
        nativeOnMoreButtonViewed(mNativeFeedLoggingBridge, position);
    }

    @Override
    public void onMoreButtonClicked(int position) {
        assert mNativeFeedLoggingBridge != 0;
        nativeOnMoreButtonClicked(mNativeFeedLoggingBridge, position);
    }

    @Override
    public void onOpenedWithContent(int timeToPopulateMs, int contentCount) {
        assert mNativeFeedLoggingBridge != 0;
        nativeOnOpenedWithContent(mNativeFeedLoggingBridge, timeToPopulateMs, contentCount);
    }

    @Override
    public void onOpenedWithNoImmediateContent() {
        assert mNativeFeedLoggingBridge != 0;
        nativeOnOpenedWithNoImmediateContent(mNativeFeedLoggingBridge);
    }

    @Override
    public void onOpenedWithNoContent() {
        assert mNativeFeedLoggingBridge != 0;
        nativeOnOpenedWithNoContent(mNativeFeedLoggingBridge);
    }

    private native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeFeedLoggingBridge);
    private native void nativeOnContentViewed(long nativeFeedLoggingBridge, int position,
            long publishedTimeSeconds, long timeContentBecameAvailable, float score);
    private native void nativeOnContentDismissed(long nativeFeedLoggingBridge, String uri);
    private native void nativeOnContentClicked(
            long nativeFeedLoggingBridge, int position, long publishedTimeSeconds, float score);
    private native void nativeOnClientAction(long nativeFeedLoggingBridge, int actionType);
    private native void nativeOnContentContextMenuOpened(
            long nativeFeedLoggingBridge, int position, long publishedTimeSeconds, float score);
    private native void nativeOnMoreButtonViewed(long nativeFeedLoggingBridge, int position);
    private native void nativeOnMoreButtonClicked(long nativeFeedLoggingBridge, int position);
    private native void nativeOnOpenedWithContent(
            long nativeFeedLoggingBridge, int timeToPopulateMs, int contentCount);
    private native void nativeOnOpenedWithNoImmediateContent(long nativeFeedLoggingBridge);
    private native void nativeOnOpenedWithNoContent(long nativeFeedLoggingBridge);
}

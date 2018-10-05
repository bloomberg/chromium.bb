// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import com.google.android.libraries.feed.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.host.logging.ContentLoggingData;

import org.chromium.chrome.browser.profiles.Profile;

/**
 * Implementation of {@link BasicLoggingApi} that log actions performed on the Feed.
 */
public class FeedBasicLogging implements BasicLoggingApi {
    private FeedLoggingBridge mFeedLoggingBridge;

    /**
     * Creates a {@link FeedBasicLogging} for accessing native logging logic.
     *
     * @param profile {@link Profile} of the user we are rendering the Feed for.
     */
    public FeedBasicLogging(Profile profile) {
        mFeedLoggingBridge = new FeedLoggingBridge(profile);
    }

    @Override
    public void onContentViewed(ContentLoggingData data) {
        mFeedLoggingBridge.onContentViewed(data.getPositionInStream(),
                data.getPublishedTimeSeconds(), data.getTimeContentBecameAvailable(),
                data.getScore());
    }

    @Override
    public void onContentDismissed(ContentLoggingData data) {
        mFeedLoggingBridge.onContentDismissed(data.getRepresentationUri());
    }

    @Override
    public void onContentClicked(ContentLoggingData data) {
        mFeedLoggingBridge.onContentClicked(
                data.getPositionInStream(), data.getPublishedTimeSeconds(), data.getScore());
    }

    @Override
    public void onClientAction(ContentLoggingData data, int actionType) {
        mFeedLoggingBridge.onClientAction(actionType);
    }

    @Override
    public void onContentContextMenuOpened(ContentLoggingData data) {
        mFeedLoggingBridge.onContentContextMenuOpened(
                data.getPositionInStream(), data.getPublishedTimeSeconds(), data.getScore());
    }

    @Override
    public void onMoreButtonViewed(int position) {
        mFeedLoggingBridge.onMoreButtonViewed(position);
    }

    @Override
    public void onMoreButtonClicked(int position) {
        mFeedLoggingBridge.onMoreButtonClicked(position);
    }

    @Override
    public void onOpenedWithContent(int timeToPopulateMs, int contentCount) {
        mFeedLoggingBridge.onOpenedWithContent(timeToPopulateMs, contentCount);
    }

    @Override
    public void onOpenedWithNoImmediateContent() {
        mFeedLoggingBridge.onOpenedWithNoImmediateContent();
    }

    @Override
    public void onOpenedWithNoContent() {
        mFeedLoggingBridge.onOpenedWithNoContent();
    }
}

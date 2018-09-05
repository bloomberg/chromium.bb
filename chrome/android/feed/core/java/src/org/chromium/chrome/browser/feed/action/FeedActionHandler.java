// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.action;

import android.support.annotation.NonNull;

import com.google.android.libraries.feed.api.knowncontent.ContentMetadata;
import com.google.android.libraries.feed.host.action.ActionApi;

import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.mojom.WindowOpenDisposition;

/**
 * Handles the actions user can trigger on the feed.
 */
public class FeedActionHandler implements ActionApi {
    private final SuggestionsNavigationDelegate mDelegate;
    private final Runnable mSuggestionConsumedObserver;

    /**
     * @param delegate The {@link SuggestionsNavigationDelegate} that this handler calls when
     *                 handling some of the actions.
     * @param suggestionConsumedObserver An observer that is interested in any time a suggestion is
     *                                   consumed by the user.
     */
    public FeedActionHandler(@NonNull SuggestionsNavigationDelegate delegate,
            @NonNull Runnable suggestionConsumedObserver) {
        mDelegate = delegate;
        mSuggestionConsumedObserver = suggestionConsumedObserver;
    }

    @Override
    public void openUrl(String url) {
        mDelegate.openUrl(WindowOpenDisposition.CURRENT_TAB, createLoadUrlParams(url));
        mSuggestionConsumedObserver.run();
    }

    @Override
    public boolean canOpenUrl() {
        return true;
    }

    @Override
    public void openUrlInIncognitoMode(String url) {
        mDelegate.openUrl(WindowOpenDisposition.OFF_THE_RECORD, createLoadUrlParams(url));
        mSuggestionConsumedObserver.run();
    }

    @Override
    public boolean canOpenUrlInIncognitoMode() {
        return mDelegate.isOpenInIncognitoEnabled();
    }

    @Override
    public void openUrlInNewTab(String url) {
        mDelegate.openUrl(WindowOpenDisposition.NEW_BACKGROUND_TAB, createLoadUrlParams(url));
        mSuggestionConsumedObserver.run();
    }

    @Override
    public boolean canOpenUrlInNewTab() {
        return true;
    }

    @Override
    public void openUrlInNewWindow(String url) {
        mDelegate.openUrl(WindowOpenDisposition.NEW_WINDOW, createLoadUrlParams(url));
        mSuggestionConsumedObserver.run();
    }

    @Override
    public boolean canOpenUrlInNewWindow() {
        return mDelegate.isOpenInNewWindowEnabled();
    }

    @Override
    public void downloadUrl(ContentMetadata contentMetadata) {
        mDelegate.openUrl(
                WindowOpenDisposition.SAVE_TO_DISK, createLoadUrlParams(contentMetadata.getUrl()));
        mSuggestionConsumedObserver.run();
    }

    @Override
    public boolean canDownloadUrl() {
        // TODO(huayinz): Change to the desired behavior.
        return false;
    }

    @Override
    public void learnMore() {
        mDelegate.navigateToHelpPage();
    }

    @Override
    public boolean canLearnMore() {
        return true;
    }

    private LoadUrlParams createLoadUrlParams(String url) {
        return new LoadUrlParams(url, PageTransition.AUTO_BOOKMARK);
    }
}

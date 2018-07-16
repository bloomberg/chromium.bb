// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import android.content.Context;
import android.view.View;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.download.home.filter.Filters;
import org.chromium.chrome.browser.download.home.filter.Filters.FilterType;
import org.chromium.chrome.browser.download.home.list.DateOrderedListCoordinator;
import org.chromium.chrome.browser.download.home.snackbars.DeleteUndoCoordinator;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.snackbar.SnackbarManager;

import java.io.Closeable;

/**
 * The top level coordinator for the download home UI.  This is currently an in progress class and
 * is not fully fleshed out yet.
 */
class DownloadManagerCoordinatorImpl implements DownloadManagerCoordinator {
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final DateOrderedListCoordinator mListCoordinator;
    private final DeleteUndoCoordinator mDeleteCoordinator;

    private boolean mMuteFilterChanges;

    /** Builds a {@link DownloadManagerCoordinatorImpl} instance. */
    public DownloadManagerCoordinatorImpl(Profile profile, Context context, boolean offTheRecord,
            SnackbarManager snackbarManager) {
        mDeleteCoordinator = new DeleteUndoCoordinator(snackbarManager);
        mListCoordinator = new DateOrderedListCoordinator(context, offTheRecord,
                OfflineContentAggregatorFactory.forProfile(profile),
                mDeleteCoordinator::showSnackbar, this::notifyFilterChanged);
    }

    // DownloadManagerCoordinator implementation.
    @Override
    public void destroy() {
        mDeleteCoordinator.destroy();
        mListCoordinator.destroy();
    }

    @Override
    public View getView() {
        return mListCoordinator.getView();
    }

    @Override
    public boolean onBackPressed() {
        // TODO(dtrainor): Clear selection if multi-select is supported.
        return false;
    }

    @Override
    public void updateForUrl(String url) {
        try (FilterChangeBlock block = new FilterChangeBlock()) {
            mListCoordinator.setSelectedFilter(Filters.fromUrl(url));
        }
    }

    @Override
    public void showPrefetchSection() {
        updateForUrl(Filters.toUrl(Filters.FilterType.PREFETCHED));
    }

    @Override
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    private void notifyFilterChanged(@FilterType int filter) {
        if (mMuteFilterChanges) return;

        String url = Filters.toUrl(filter);
        for (Observer observer : mObservers) observer.onUrlChanged(url);
    }

    /**
     * Helper class to mute state changes when processing a state change request from an external
     * source.
     */
    private class FilterChangeBlock implements Closeable {
        private final boolean mOriginalMuteFilterChanges;

        public FilterChangeBlock() {
            mOriginalMuteFilterChanges = mMuteFilterChanges;
            mMuteFilterChanges = true;
        }

        @Override
        public void close() {
            mMuteFilterChanges = mOriginalMuteFilterChanges;
        }
    }
}
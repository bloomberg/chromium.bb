// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.browser.download.home.filter.FilterCoordinator;
import org.chromium.components.offline_items_collection.OfflineContentProvider;

/**
 * The top level coordinator for the download home UI.  This is currently an in progress class and
 * is not fully fleshed out yet.
 */
public class DateOrderedListCoordinator {
    private final FilterCoordinator mFilterCoordinator;

    private final DateOrderedListModel mModel;
    private final DateOrderedListMediator mMediator;
    private final DateOrderedListView mView;

    /** Creates an instance of a DateOrderedListCoordinator, which will visually represent
     * {@code provider} as a list of items.
     * @param context  The {@link Context} to use to build the views.
     * @param provider The {@link OfflineContentProvider} to visually represent.
     */
    public DateOrderedListCoordinator(Context context, OfflineContentProvider provider) {
        mModel = new DateOrderedListModel();
        mMediator = new DateOrderedListMediator(provider, mModel);
        mView = new DateOrderedListView(context, mModel);

        // Hook up the FilterCoordinator with our mediator.
        mFilterCoordinator = new FilterCoordinator(context, mMediator.getFilterSource());
        mFilterCoordinator.addObserver(filter -> mMediator.onFilterTypeSelected(filter));
    }

    /** Tears down this coordinator. */
    public void destroy() {
        mMediator.destroy();
    }

    /** @return The {@link View} representing downloads home. */
    public View getView() {
        return mView.getView();
    }
}
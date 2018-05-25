// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import org.chromium.chrome.browser.download.home.OfflineItemSource;
import org.chromium.chrome.browser.download.home.filter.DeleteUndoOfflineItemFilter;
import org.chromium.chrome.browser.download.home.filter.Filters.FilterType;
import org.chromium.chrome.browser.download.home.filter.OfflineItemFilterSource;
import org.chromium.chrome.browser.download.home.filter.SearchOfflineItemFilter;
import org.chromium.chrome.browser.download.home.filter.TypeOfflineItemFilter;
import org.chromium.components.offline_items_collection.OfflineContentProvider;

/**
 * A Mediator responsible for converting an OfflineContentProvider to a list of items in downloads
 * home.  This includes support for filtering, deleting, etc..
 */
class DateOrderedListMediator {
    private final OfflineContentProvider mProvider;
    private final DateOrderedListModel mModel;

    private final OfflineItemSource mSource;
    private final DateOrderedListMutator mListMutator;

    private final DeleteUndoOfflineItemFilter mDeleteUndoFilter;
    private final TypeOfflineItemFilter mTypeFilter;
    private final SearchOfflineItemFilter mSearchFilter;

    /**
     * Creates an instance of a DateOrderedListMediator that will push {@code provider} into
     * {@code model}.
     * @param provider The {@link OfflineContentProvider} to visually represent.
     * @param model    The {@link DateOrderedListModel} to push {@code provider} into.
     */
    public DateOrderedListMediator(OfflineContentProvider provider, DateOrderedListModel model) {
        // Build a chain from the data source to the model.  The chain will look like:
        // [OfflineContentProvider] ->
        //     [OfflineItemSource] ->
        //         [DeleteUndoOfflineItemFilter] ->
        //             [TypeOfflineItemFilter] ->
        //                 [SearchOfflineItemFitler] ->
        //                     [DateOrderedListMutator] ->
        //                         [DateOrderedListModel]

        mProvider = provider;
        mModel = model;

        mSource = new OfflineItemSource(mProvider);
        mDeleteUndoFilter = new DeleteUndoOfflineItemFilter(mSource);
        mTypeFilter = new TypeOfflineItemFilter(mDeleteUndoFilter);
        mSearchFilter = new SearchOfflineItemFilter(mTypeFilter);
        mListMutator = new DateOrderedListMutator(mSearchFilter, mModel);
    }

    /**
     * To be called when this mediator should filter its content based on {@code filter}.
     * @see TypeOfflineItemFilter#onFilterSelected(int)
     */
    public void onFilterTypeSelected(@FilterType int filter) {
        mTypeFilter.onFilterSelected(filter);
    }

    /**
     * To be called when this mediator should filter its content based on {@code filter}.
     * @see SearchOfflineItemFilter#onQueryChanged(String)
     */
    public void onFilterStringChanged(String filter) {
        mSearchFilter.onQueryChanged(filter);
    }

    /**
     * @return The {@link OfflineItemFilterSource} that should be used to determine which filter
     *         options are available.
     */
    public OfflineItemFilterSource getFilterSource() {
        return mDeleteUndoFilter;
    }
}
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.content.Context;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.View;

import org.chromium.chrome.browser.modelutil.RecyclerViewModelChangeProcessor;

/**
 * The View component of a DateOrderedList.  This takes the DateOrderedListModel and creates the
 * glue to display it on the screen.
 */
class DateOrderedListView {
    private final DateOrderedListModel mModel;
    private final RecyclerView mView;

    /** Creates an instance of a {@link DateOrderedListView} representing {@code model}. */
    public DateOrderedListView(Context context, DateOrderedListModel model) {
        mModel = model;

        DateOrderedListViewBinder viewBinder = new DateOrderedListViewBinder();
        DateOrderedListViewAdapter adapter = new DateOrderedListViewAdapter(mModel, viewBinder);
        RecyclerViewModelChangeProcessor<DateOrderedListModel, ViewHolder> modelChangeProcessor =
                new RecyclerViewModelChangeProcessor<>(adapter);

        mModel.addObserver(modelChangeProcessor);

        mView = new RecyclerView(context);
        mView.setHasFixedSize(true);
        mView.getItemAnimator().setChangeDuration(0);
        mView.setLayoutManager(
                new LinearLayoutManager(context, LinearLayoutManager.VERTICAL, false));
        mView.setAdapter(adapter);
    }

    /** @return The Android {@link View} representing this widget. */
    public View getView() {
        return mView;
    }
}
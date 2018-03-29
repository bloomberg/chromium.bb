// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.view.View.OnClickListener;

import org.chromium.chrome.browser.modelutil.ListObservable;
import org.chromium.chrome.browser.modelutil.PropertyObservable;

import java.util.Collections;

/** A model for the contextual suggestions UI component. */
class ContextualSuggestionsModel extends PropertyObservable<PropertyKey> {
    /** A {@link ListObservable} containing the current cluster list. */
    class ClusterListObservable extends ListObservable {
        ClusterList mClusterList = new ClusterList(Collections.emptyList());

        private void setClusterList(ClusterList clusterList) {
            assert clusterList != null;

            int oldLength = getItemCount();
            mClusterList = clusterList;

            if (oldLength != 0) notifyItemRangeRemoved(0, oldLength);
            if (getItemCount() != 0) notifyItemRangeInserted(0, getItemCount());
        }

        @Override
        public int getItemCount() {
            return mClusterList.getItemCount();
        }
    }

    ClusterListObservable mClusterListObservable = new ClusterListObservable();
    private OnClickListener mCloseButtonOnClickListener;
    private String mTitle;

    /** @param suggestions The current list of clusters. */
    void setClusterList(ClusterList clusterList) {
        mClusterListObservable.setClusterList(clusterList);
    }

    /** @return The current list of clusters. */
    ClusterList getClusterList() {
        return mClusterListObservable.mClusterList;
    }

    /** @param listener The {@link OnClickListener} for the close button. */
    void setCloseButtonOnClickListener(OnClickListener listener) {
        mCloseButtonOnClickListener = listener;
        notifyPropertyChanged(new PropertyKey(PropertyKey.CLOSE_BUTTON_ON_CLICK_LISTENER));
    }

    /** @return The {@link OnClickListener} for the close button. */
    OnClickListener getCloseButtonOnClickListener() {
        return mCloseButtonOnClickListener;
    }

    /** @param title The title to display in the toolbar. */
    void setTitle(String title) {
        mTitle = title;
        notifyPropertyChanged(new PropertyKey(PropertyKey.TITLE));
    }

    /** @return title The title to display in the toolbar. */
    String getTitle() {
        return mTitle;
    }

    /**
     * @return Whether there are any suggestions to be shown.
     */
    boolean hasSuggestions() {
        return getClusterList().getItemCount() > 0;
    }
}

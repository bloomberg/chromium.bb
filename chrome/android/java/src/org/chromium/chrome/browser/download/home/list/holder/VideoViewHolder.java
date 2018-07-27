// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.holder;

import android.support.v7.widget.AppCompatTextView;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.ListPropertyModel;

/**
 * A {@link RecyclerView.ViewHolder} specifically meant to display a video {@code OfflineItem}.
 */
public class VideoViewHolder extends ListItemViewHolder {
    public VideoViewHolder(ViewGroup parent) {
        super(new AppCompatTextView(parent.getContext()));
    }

    // ListItemViewHolder implementation.
    @Override
    public void bind(ListPropertyModel properties, ListItem item) {
        ListItem.OfflineItemListItem offlineItem = (ListItem.OfflineItemListItem) item;
        ((TextView) itemView).setText(offlineItem.item.title);
    }
}

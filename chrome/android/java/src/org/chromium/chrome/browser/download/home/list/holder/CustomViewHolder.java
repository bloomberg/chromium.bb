// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.holder;

import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.ListPropertyModel;

/** A {@link RecyclerView.ViewHolder} that holds a {@link View} that is opaque to the holder. */
public class CustomViewHolder extends ListItemViewHolder {
    /** Creates a new {@link org.chromium.chrome.browser.download.home.list.holder.CustomViewHolder}
     * instance. */
    public CustomViewHolder(ViewGroup parent) {
        super(new FrameLayout(parent.getContext()));
        itemView.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
    }

    // ListItemViewHolder implemenation.
    @Override
    public void bind(ListPropertyModel properties, ListItem item) {
        ListItem.ViewListItem viewItem = (ListItem.ViewListItem) item;
        ViewGroup viewGroup = (ViewGroup) itemView;

        if (viewGroup.getChildCount() > 0 && viewGroup.getChildAt(0) == viewItem.customView) {
            return;
        }

        viewGroup.removeAllViews();
        viewGroup.addView(viewItem.customView,
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
    }
}

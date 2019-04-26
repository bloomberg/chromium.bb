// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;

import org.chromium.chrome.tab_ui.R;

/**
 * {@link RecyclerView.ViewHolder} for tab strip.
 */
class TabStripViewHolder extends RecyclerView.ViewHolder {
    private int mTabId;
    public final ImageButton button;

    private TabStripViewHolder(View itemView) {
        super(itemView);
        this.button = itemView.findViewById(R.id.tab_strip_item_button);
    }

    public static TabStripViewHolder create(ViewGroup parent, int itemViewType) {
        View view = LayoutInflater.from(parent.getContext())
                            .inflate(R.layout.tab_strip_item, parent, false);
        return new TabStripViewHolder(view);
    }

    public void setTabId(int tabId) {
        mTabId = tabId;
    }

    public int getTabId() {
        return mTabId;
    }
}

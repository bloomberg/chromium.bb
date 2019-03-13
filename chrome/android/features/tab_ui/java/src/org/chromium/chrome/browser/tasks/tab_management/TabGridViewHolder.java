// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.support.v4.graphics.drawable.DrawableCompat;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;

/**
 * {@link RecyclerView.ViewHolder} for tab grid. Owns the tab info card
 * and the associated view hierarchy.
 */
class TabGridViewHolder extends RecyclerView.ViewHolder {
    public final ImageView favicon;
    public final TextView title;
    public final ImageView thumbnail;
    public final ImageView closeButton;
    private int mTabId;

    private TabGridViewHolder(View itemView) {
        super(itemView);
        this.thumbnail = itemView.findViewById(R.id.tab_thumbnail);
        this.title = itemView.findViewById(R.id.tab_title);
        this.favicon = itemView.findViewById(R.id.tab_favicon);
        this.closeButton = itemView.findViewById(R.id.close_button);
        DrawableCompat.setTint(this.closeButton.getDrawable(),
                ApiCompatibilityUtils.getColor(itemView.getResources(), R.color.light_icon_color));
    }

    public static TabGridViewHolder create(ViewGroup parent, int itemViewType) {
        View view =
                LayoutInflater.from(parent.getContext())
                        .inflate(org.chromium.chrome.R.layout.tab_grid_card_item, parent, false);
        return new TabGridViewHolder(view);
    }

    public void setTabId(int tabId) {
        mTabId = tabId;
    }

    public int getTabId() {
        return mTabId;
    }
}

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.support.v7.widget.RecyclerView;
import android.view.View;

/**
 * Holds metadata about an item we want to display on the NTP. An item can be anything that will be
 * displayed on the NTP RecyclerView.
 */
public class NewTabPageViewHolder extends RecyclerView.ViewHolder {
    /**
     * Constructs a NewTabPageViewHolder item used to display an part of the NTP (e.g., header,
     * article snippet, above the fold view, etc.)
     *
     * @param itemView The View for this item
     */
    public NewTabPageViewHolder(View itemView) {
        super(itemView);
    }

    /**
     * Called when the NTP cards adapter is requested to update the currently visible ViewHolder
     * with data. The default implementation does nothing.
     *
     * @param item The NewTabPageListItem object that holds the data for this ViewHolder
     */
    public void onBindViewHolder(NewTabPageListItem item) {
    }

    /**
     * Whether this item can be swiped and dismissed. The default implementation disallows it.
     * @return {@code true} if the item can be swiped and dismissed, {@code false} otherwise.
     */
    public boolean isDismissable() {
        return false;
    }

    /**
     * Update the wrapped view's state as it is being swiped away.
     * @param dX The amount of horizontal displacement caused by user's action
     */
    public void updateViewStateForDismiss(float dX) {}

    /**
     * Update the layout params for the view holder.
     */
    public void updateLayoutParams() {
    }
}

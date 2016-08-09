// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.NewTabPageView.NewTabPageManager;
import org.chromium.chrome.browser.ntp.UiConfig;

/**
 * Item that allows the user to perform an action on the NTP.
 */
class ActionListItem implements NewTabPageListItem {
    @Override
    public int getType() {
        return NewTabPageListItem.VIEW_TYPE_ACTION;
    }

    public static class ViewHolder extends CardViewHolder {
        public ViewHolder(NewTabPageRecyclerView recyclerView, final NewTabPageManager manager,
                UiConfig uiConfig) {
            super(R.layout.new_tab_page_action_card, recyclerView, uiConfig);
            itemView.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    // TODO(dgn): Implement other behaviours.
                    manager.navigateToBookmarks();
                }
            });
        }
    }
}

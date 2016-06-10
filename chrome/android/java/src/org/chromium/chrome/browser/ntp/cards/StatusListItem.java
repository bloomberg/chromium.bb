// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;

import org.chromium.chrome.R;

/**
 * Card that is shown when the user needs to be made aware of some information about their
 * configuration about the NTP suggestions: there is no more available suggested content, sync
 * should be enabled, etc.
 */
public class StatusListItem implements NewTabPageListItem {
    /**
     * ViewHolder for an item of type {@link #VIEW_TYPE_STATUS}.
     */
    public static class ViewHolder extends CardViewHolder {
        public ViewHolder(NewTabPageRecyclerView parent, final NewTabPageAdapter adapter) {
            super(R.layout.new_tab_page_status_card, parent);
            Button reloadButton = ((Button) itemView.findViewById(R.id.reload_button));
            reloadButton.setOnClickListener(new OnClickListener() {

                @Override
                public void onClick(View v) {
                    adapter.reloadSnippets();
                }
            });
        }
    }

    @Override
    public int getType() {
        return NewTabPageListItem.VIEW_TYPE_STATUS;
    }
}

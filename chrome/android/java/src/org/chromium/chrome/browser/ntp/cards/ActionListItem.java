// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.view.View;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.NewTabPageView.NewTabPageManager;
import org.chromium.chrome.browser.ntp.UiConfig;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;

/**
 * Item that allows the user to perform an action on the NTP.
 */
class ActionListItem implements NewTabPageListItem {
    private static final String TAG = "NtpCards";

    private final int mCategory;

    public ActionListItem(int category) {
        mCategory = category;
    }

    @Override
    public int getType() {
        return NewTabPageListItem.VIEW_TYPE_ACTION;
    }

    public static class ViewHolder extends CardViewHolder {
        private ActionListItem mActionListItem;

        public ViewHolder(NewTabPageRecyclerView recyclerView, final NewTabPageManager manager,
                UiConfig uiConfig) {
            super(R.layout.new_tab_page_action_card, recyclerView, uiConfig);
            itemView.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    int category = mActionListItem.mCategory;
                    if (category == KnownCategories.BOOKMARKS) {
                        manager.navigateToBookmarks();
                    } else if (category == KnownCategories.DOWNLOADS) {
                        manager.navigateToDownloadManager();
                    } else {
                        // TODO(pke): This should redirect to the C++ backend. Once it does,
                        // change the condition in the SuggestionsSection constructor.
                        Log.wtf(TAG, "More action called on a dynamically added section: %d",
                                category);
                    }
                }
            });
        }

        @Override
        public void onBindViewHolder(NewTabPageListItem item) {
            super.onBindViewHolder(item);
            mActionListItem = (ActionListItem) item;
        }
    }
}

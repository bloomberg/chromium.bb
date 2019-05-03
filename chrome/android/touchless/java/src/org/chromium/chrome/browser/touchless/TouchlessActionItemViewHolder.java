// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.ntp.cards.ActionItem;
import org.chromium.chrome.browser.suggestions.SuggestionsRecyclerView;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;
import org.chromium.chrome.touchless.R;
import org.chromium.ui.widget.ChromeImageView;

/** ViewHolder associated to {@link ItemViewType#ACTION} for touchless devices. */
class TouchlessActionItemViewHolder extends ActionItem.ViewHolder {
    private TextView mTextView;
    private ChromeImageView mImageView;

    TouchlessActionItemViewHolder(SuggestionsRecyclerView recyclerView,
            ContextMenuManager contextMenuManager, final SuggestionsUiDelegate uiDelegate,
            UiConfig uiConfig) {
        super(R.layout.dialog_list_item, recyclerView, contextMenuManager, uiDelegate, uiConfig);
        mTextView = itemView.findViewById(R.id.dialog_item_text);
        mImageView = itemView.findViewById(R.id.dialog_item_icon);
    }

    @Override
    public void onBindViewHolder(ActionItem item) {
        super.onBindViewHolder(item);

        LinearLayout.LayoutParams params =
                new LinearLayout.LayoutParams(itemView.getLayoutParams());
        params.bottomMargin = itemView.getResources().getDimensionPixelSize(
                R.dimen.touchless_new_tab_recycler_view_over_scroll);
        itemView.setLayoutParams(params);
        itemView.setBackground(ApiCompatibilityUtils.getDrawable(
                itemView.getResources(), R.drawable.hairline_border_card_background));
        mTextView.setText(itemView.getResources().getString(R.string.more_articles));
        mImageView.setImageDrawable(
                ApiCompatibilityUtils.getDrawable(itemView.getResources(), R.drawable.ic_note_add));
        // Image view defaults to GONE.
        mImageView.setVisibility(View.VISIBLE);
    }

    @Override
    protected void setState(@ActionItem.State int state) {
        assert state != ActionItem.State.HIDDEN;

        // Similar to the method in ActionItem.ViewHolder, but removing the animted progress
        // indicator that's not supported for touchless devices.
        if (state == ActionItem.State.BUTTON) {
            mButton.setVisibility(View.VISIBLE);
        } else if (state == ActionItem.State.INITIAL_LOADING
                || state == ActionItem.State.MORE_BUTTON_LOADING) {
            mButton.setVisibility(View.INVISIBLE);
        } else {
            // Not even HIDDEN is supported as the item should not be able to receive updates.
            assert false : "ActionViewHolder got notified of an unsupported state: " + state;
        }
    }
}

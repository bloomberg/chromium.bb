// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.support.annotation.ColorRes;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.cards.CardViewHolder;
import org.chromium.chrome.browser.ntp.cards.ItemViewType;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;
import org.chromium.chrome.browser.ntp.cards.NodeVisitor;
import org.chromium.chrome.browser.ntp.cards.OptionalLeaf;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;

/**
 * Optional node describing an empty content suggestion card.
 */
public class ContentSuggestionPlaceholder extends OptionalLeaf {
    /** Color used to fill highlighted empty UI elements of the placeholder. */
    private static final @ColorRes int HIGHLIGHT_COLOR = R.color.black_alpha_20;

    public void setVisible(boolean visible) {
        setVisibilityInternal(visible);
    }

    @Override
    protected void onBindViewHolder(NewTabPageViewHolder holder) {
        ((ViewHolder) holder).onBindViewHolder();
    }

    @Override
    protected int getItemViewType() {
        return ItemViewType.PLACEHOLDER_CARD;
    }

    @Override
    protected void visitOptionalItem(NodeVisitor visitor) {
        visitor.visitPlaceholderItem();
    }

    /**
     * ViewHolder for {@link ContentSuggestionPlaceholder}. Holds an empty modern card, where the
     * main fields (thumbnail, title and publisher) are filled with some plain color instead of
     * actual data.
     */
    public static class ViewHolder extends CardViewHolder {
        public ViewHolder(SuggestionsRecyclerView recyclerView, UiConfig uiConfig,
                ContextMenuManager contextMenuManager) {
            super(R.layout.content_suggestions_card_modern, recyclerView, uiConfig,
                    contextMenuManager);

            itemView.findViewById(R.id.article_thumbnail).setBackgroundResource(HIGHLIGHT_COLOR);
        }

        @Override
        public boolean isDismissable() {
            return false;
        }

        @Override
        public boolean isItemSupported(int menuItemId) {
            return false;
        }
    }
}

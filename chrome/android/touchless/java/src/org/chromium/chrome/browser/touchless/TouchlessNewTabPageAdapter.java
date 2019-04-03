// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.support.annotation.Nullable;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.ntp.cards.ItemViewType;
import org.chromium.chrome.browser.ntp.cards.NewTabPageAdapter;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Adds touchless specific logic that needs to be attached and/or interacted with view holders.
 */
public class TouchlessNewTabPageAdapter extends NewTabPageAdapter {
    private PropertyModel mModel;

    // Initial focus should only be set once. After that happens this sentinel is sent to false and
    // the initial focus field should not be used.
    private boolean mCheckInitialFocus = true;

    /**
     * Creates the adapter that will manage all the cards to display on the NTP.
     * @param uiDelegate used to interact with the rest of the system.
     * @param aboveTheFoldView the layout encapsulating all the above-the-fold elements
     *         (logo, search box, most visited tiles), or null if only suggestions should
     *         be displayed.
     * @param uiConfig the NTP UI configuration, to be passed to created views.
     * @param offlinePageBridge used to determine if articles are available.
     * @param contextMenuManager used to build context menus.
     * @param model the model of the recycler view component.
     */
    public TouchlessNewTabPageAdapter(SuggestionsUiDelegate uiDelegate,
            @Nullable View aboveTheFoldView, UiConfig uiConfig, OfflinePageBridge offlinePageBridge,
            ContextMenuManager contextMenuManager, PropertyModel model) {
        super(uiDelegate, aboveTheFoldView, uiConfig, offlinePageBridge, contextMenuManager);
        mModel = model;
    }

    @Override
    public void onBindViewHolder(NewTabPageViewHolder holder, int position, List<Object> payloads) {
        super.onBindViewHolder(holder, position, payloads);

        Callback<TouchlessNewTabPageFocusInfo> focusCallback =
                mModel.get(TouchlessNewTabPageProperties.FOCUS_CHANGE_CALLBACK);
        if (focusCallback != null && holder.getItemViewType() == ItemViewType.SNIPPET) {
            holder.itemView.setOnFocusChangeListener((View view, boolean hasFocus) -> {
                if (hasFocus) {
                    focusCallback.onResult(new TouchlessNewTabPageFocusInfo(
                            TouchlessNewTabPageFocusInfo.FocusType.ARTICLE,
                            holder.getAdapterPosition()));

                    // If focus is changing due to user interactions, the initial focus is no longer
                    // needed. While this flag is typically flipped when the initial focus is set,
                    // if the initial focus is not restored successfully, for example when the
                    // initial focus would be on an index larger than the initial number of cards,
                    // then it needs to be cleared here.
                    mCheckInitialFocus = false;
                }
            });
        }

        TouchlessNewTabPageFocusInfo focus =
                mModel.get(TouchlessNewTabPageProperties.INITIAL_FOCUS);
        if (mCheckInitialFocus && focus != null && focus.index == holder.getAdapterPosition()
                && focus.type == holder.getItemViewType()) {
            holder.itemView.requestFocus();
            mCheckInitialFocus = false;
        }
    }
}
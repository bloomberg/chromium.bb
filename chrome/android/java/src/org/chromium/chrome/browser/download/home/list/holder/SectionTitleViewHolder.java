// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.holder;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.ListProperties;
import org.chromium.chrome.browser.download.home.list.ListUtils;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.widget.ListMenuButton;
import org.chromium.chrome.download.R;
import org.chromium.components.offline_items_collection.OfflineItemFilter;

/**
 * A {@link ViewHolder} specifically meant to display a section header.
 */
public class SectionTitleViewHolder extends ListItemViewHolder implements ListMenuButton.Delegate {
    private final TextView mTitle;
    private final ListMenuButton mMore;

    private Runnable mShareCallback;
    private Runnable mDeleteCallback;
    private Runnable mShareAllCallback;
    private Runnable mDeleteAllCallback;
    private Runnable mSelectCallback;

    private boolean mListHasMultipleItems;

    /** Create a new {@link SectionTitleViewHolder} instance. */
    public static SectionTitleViewHolder create(ViewGroup parent) {
        View view = LayoutInflater.from(parent.getContext())
                            .inflate(R.layout.download_manager_section_header, null);
        return new SectionTitleViewHolder(view);
    }

    private SectionTitleViewHolder(View view) {
        super(view);
        mTitle = (TextView) view.findViewById(R.id.title);
        mMore = (ListMenuButton) view.findViewById(R.id.more);
        if (mMore != null) mMore.setDelegate(this);
    }

    // ListItemViewHolder implementation.
    @Override
    public void bind(PropertyModel properties, ListItem item) {
        ListItem.SectionHeaderListItem sectionItem = (ListItem.SectionHeaderListItem) item;
        mTitle.setText(ListUtils.getTextForSection(sectionItem.filter));

        boolean isPhoto = sectionItem.filter == OfflineItemFilter.FILTER_IMAGE;
        Resources resources = itemView.getContext().getResources();

        int paddingTop = resources.getDimensionPixelSize(isPhoto
                        ? R.dimen.download_manager_section_title_padding_image
                        : R.dimen.download_manager_section_title_padding_top);
        int paddingBottom = resources.getDimensionPixelSize(isPhoto
                        ? R.dimen.download_manager_section_title_padding_image
                        : R.dimen.download_manager_section_title_padding_bottom);

        if (sectionItem.isFirstSectionOfDay) {
            paddingTop = resources.getDimensionPixelSize(
                    R.dimen.download_manager_section_title_padding_top_condensed);
        }

        mTitle.setPadding(
                mTitle.getPaddingLeft(), paddingTop, mTitle.getPaddingRight(), paddingBottom);

        if (mMore != null) mMore.setVisibility(isPhoto ? View.VISIBLE : View.GONE);

        mListHasMultipleItems = sectionItem.items.size() > 1;

        if (isPhoto && mMore != null) {
            assert sectionItem.items.size() > 0;
            mShareCallback = ()
                    -> properties.get(ListProperties.CALLBACK_SHARE)
                               .onResult(sectionItem.items.get(0));
            mDeleteCallback = ()
                    -> properties.get(ListProperties.CALLBACK_REMOVE)
                               .onResult(sectionItem.items.get(0));

            mShareAllCallback = ()
                    -> properties.get(ListProperties.CALLBACK_SHARE_ALL)
                               .onResult(sectionItem.items);
            mDeleteAllCallback = ()
                    -> properties.get(ListProperties.CALLBACK_REMOVE_ALL)
                               .onResult(sectionItem.items);
            mSelectCallback =
                    () -> properties.get(ListProperties.CALLBACK_SELECTION).onResult(null);

            mMore.setClickable(!properties.get(ListProperties.SELECTION_MODE_ACTIVE));
        }
    }

    @Override
    public ListMenuButton.Item[] getItems() {
        Context context = itemView.getContext();
        if (mListHasMultipleItems) {
            return new ListMenuButton.Item[] {
                    new ListMenuButton.Item(context, R.string.select, true),
                    new ListMenuButton.Item(context, R.string.share_group, true),
                    new ListMenuButton.Item(context, R.string.delete_group, true)};
        } else {
            return new ListMenuButton.Item[] {
                    new ListMenuButton.Item(context, R.string.share, true),
                    new ListMenuButton.Item(context, R.string.delete, true)};
        }
    }

    @Override
    public void onItemSelected(ListMenuButton.Item item) {
        if (item.getTextId() == R.string.select) {
            mSelectCallback.run();
        } else if (item.getTextId() == R.string.share) {
            mShareCallback.run();
        } else if (item.getTextId() == R.string.delete) {
            mDeleteCallback.run();
        } else if (item.getTextId() == R.string.share_group) {
            mShareAllCallback.run();
        } else if (item.getTextId() == R.string.delete_group) {
            mDeleteAllCallback.run();
        }
    }
}

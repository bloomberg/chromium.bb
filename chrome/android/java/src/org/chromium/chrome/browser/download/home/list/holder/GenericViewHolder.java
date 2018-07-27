// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.holder;

import android.support.annotation.DrawableRes;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.ListPropertyModel;
import org.chromium.chrome.browser.download.home.list.UiUtils;
import org.chromium.chrome.browser.download.home.view.GenericListItemView;
import org.chromium.components.offline_items_collection.OfflineItemVisuals;

/** A {@link RecyclerView.ViewHolder} specifically meant to display a generic {@code OfflineItem}.
 */
public class GenericViewHolder extends ThumbnailAwareViewHolder {
    private static final int INVALID_ID = -1;

    private final TextView mTitle;
    private final TextView mCaption;

    /**
     * Whether or not we are currently showing an icon.  This determines whether or not we
     * udpate the icon on rebind.
     */
    private boolean mDrawingIcon;

    /** The icon to use when there is no thumbnail. */
    private @DrawableRes int mIconId = INVALID_ID;

    /** Creates a new {@link
     * org.chromium.chrome.browser.download.home.list.holder.GenericViewHolder} instance. */
    public static org.chromium.chrome.browser.download.home.list.holder.GenericViewHolder create(
            ViewGroup parent) {
        View view = LayoutInflater.from(parent.getContext())
                            .inflate(R.layout.download_manager_generic_item, null);
        int imageSize = parent.getContext().getResources().getDimensionPixelSize(
                R.dimen.download_manager_generic_thumbnail_size);
        return new org.chromium.chrome.browser.download.home.list.holder.GenericViewHolder(
                view, imageSize);
    }

    private GenericViewHolder(View view, int thumbnailSizePx) {
        super(view, thumbnailSizePx, thumbnailSizePx);

        mTitle = (TextView) itemView.findViewById(R.id.title);
        mCaption = (TextView) itemView.findViewById(R.id.caption);
    }

    // ListItemViewHolder implementation.
    @Override
    public void bind(ListPropertyModel properties, ListItem item) {
        super.bind(properties, item);
        ListItem.OfflineItemListItem offlineItem = (ListItem.OfflineItemListItem) item;

        mTitle.setText(offlineItem.item.title);
        mCaption.setText(UiUtils.generateGenericCaption(offlineItem.item));

        mIconId = UiUtils.getIconForItem(offlineItem.item);
    }

    @Override
    void onVisualsChanged(ImageView view, OfflineItemVisuals visuals) {
        mDrawingIcon = visuals == null || visuals.icon == null;

        GenericListItemView selectableView = (GenericListItemView) itemView;
        if (mDrawingIcon) {
            if (mIconId != INVALID_ID) {
                selectableView.setThumbnailResource(mIconId);
            }
        } else {
            selectableView.setThumbnail(visuals.icon);
        }
    }
}

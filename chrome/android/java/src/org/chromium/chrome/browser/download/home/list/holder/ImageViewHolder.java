// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.holder;

import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.support.graphics.drawable.AnimatedVectorDrawableCompat;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.ListPropertyModel;
import org.chromium.chrome.browser.widget.TintedImageView;
import org.chromium.components.offline_items_collection.OfflineItemVisuals;

/** A {@link RecyclerView.ViewHolder} specifically meant to display an image {@code OfflineItem}. */
public class ImageViewHolder extends ThumbnailAwareViewHolder {
    private final TintedImageView mSelectionImage;
    private final ColorStateList mCheckedIconForegroundColorList;
    private final AnimatedVectorDrawableCompat mCheckDrawable;

    public static org.chromium.chrome.browser.download.home.list.holder.ImageViewHolder create(
            ViewGroup parent) {
        View view = LayoutInflater.from(parent.getContext())
                            .inflate(R.layout.download_manager_image_item, null);
        int imageSize = parent.getContext().getResources().getDimensionPixelSize(
                R.dimen.download_manager_image_width);
        return new org.chromium.chrome.browser.download.home.list.holder.ImageViewHolder(
                view, imageSize);
    }

    public ImageViewHolder(View view, int thumbnailSizePx) {
        super(view, thumbnailSizePx, thumbnailSizePx);
        mCheckDrawable = AnimatedVectorDrawableCompat.create(
                itemView.getContext(), R.drawable.ic_check_googblue_24dp_animated);
        mCheckedIconForegroundColorList =
                DownloadUtils.getIconForegroundColorList(itemView.getContext());
        mSelectionImage = (TintedImageView) itemView.findViewById(R.id.selection);
    }

    // ThumbnailAwareViewHolder implementation.
    @Override
    public void bind(ListPropertyModel properties, ListItem item) {
        super.bind(properties, item);
        ListItem.OfflineItemListItem offlineItem = (ListItem.OfflineItemListItem) item;
        View imageView = itemView.findViewById(R.id.thumbnail);
        imageView.setContentDescription(offlineItem.item.title);
        updateImageView();
    }

    @Override
    void onVisualsChanged(ImageView view, OfflineItemVisuals visuals) {
        view.setImageBitmap(visuals == null ? null : visuals.icon);
        updateImageView();
    }

    protected void updateImageView() {
        Resources resources = itemView.getContext().getResources();
        // TODO(shaktisahu): Pass the appropriate value of selection.
        boolean selected = false;
        boolean selectionModeActive = false;
        if (selected) {
            mSelectionImage.setVisibility(View.VISIBLE);
            mSelectionImage.setBackgroundResource(R.drawable.list_item_icon_modern_bg);
            mSelectionImage.getBackground().setLevel(
                    resources.getInteger(R.integer.list_item_level_selected));

            mSelectionImage.setImageDrawable(mCheckDrawable);
            mSelectionImage.setTint(mCheckedIconForegroundColorList);
            mCheckDrawable.start();
        } else if (selectionModeActive) {
            mSelectionImage.setVisibility(View.VISIBLE);
            mSelectionImage.setBackground(null);
            mSelectionImage.setTint(null);

            mSelectionImage.setImageResource(R.drawable.download_circular_selector_transparent);
        } else {
            mSelectionImage.setBackground(null);
            mSelectionImage.setTint(null);
            mSelectionImage.setVisibility(View.GONE);
        }
    }
}

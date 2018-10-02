// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.holder;

import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.ListProperties;
import org.chromium.chrome.browser.download.home.list.view.AsyncImageView;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.download.R;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItem;

/** A {@link RecyclerView.ViewHolder} specifically meant to display an image {@code OfflineItem}. */
public class ImageViewHolder extends ListItemViewHolder {
    private final AsyncImageView mThumbnail;

    private ContentId mBoundItemId;

    public static ImageViewHolder create(ViewGroup parent) {
        View view = LayoutInflater.from(parent.getContext())
                            .inflate(R.layout.download_manager_image_item, null);
        return new ImageViewHolder(view);
    }

    public ImageViewHolder(View view) {
        super(view);
        mThumbnail = itemView.findViewById(R.id.thumbnail);
    }

    // ListItemViewHolder implementation.
    @Override
    public void bind(PropertyModel properties, ListItem item) {
        OfflineItem offlineItem = ((ListItem.OfflineItemListItem) item).item;

        mThumbnail.setContentDescription(offlineItem.title);

        if (offlineItem.id.equals(mBoundItemId)) return;
        mBoundItemId = offlineItem.id;

        mThumbnail.setAsyncImageDrawable((consumer, width, height) -> {
            return properties.get(ListProperties.PROVIDER_VISUALS)
                    .getVisuals(offlineItem, width, height, (id, visuals) -> {
                        if (!id.equals(mBoundItemId)) return;

                        Drawable drawable = null;
                        if (visuals != null && visuals.icon != null) {
                            drawable = new BitmapDrawable(itemView.getResources(), visuals.icon);
                        }
                        consumer.onResult(drawable);
                    });
        });
    }
}

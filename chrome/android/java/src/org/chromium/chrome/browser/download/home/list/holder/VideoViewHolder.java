// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.holder;

import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.ListProperties;
import org.chromium.chrome.browser.download.home.list.UiUtils;
import org.chromium.chrome.browser.download.home.list.view.AsyncImageView;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.download.R;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItem;

/**
 * A {@link RecyclerView.ViewHolder} specifically meant to display a video {@code OfflineItem}.
 */
public class VideoViewHolder extends MoreButtonViewHolder {
    private final TextView mTitle;
    private final TextView mCaption;
    private final AsyncImageView mThumbnail;

    private ContentId mBoundItemId;

    /**
     * Creates a new {@link VideoViewHolder} instance.
     */
    public static VideoViewHolder create(ViewGroup parent) {
        View view = LayoutInflater.from(parent.getContext())
                            .inflate(R.layout.download_manager_video_item, null);

        return new VideoViewHolder(view);
    }

    public VideoViewHolder(View view) {
        super(view);

        mTitle = itemView.findViewById(R.id.title);
        mCaption = itemView.findViewById(R.id.caption);
        mThumbnail = itemView.findViewById(R.id.thumbnail);
    }

    // MoreButtonViewHolder implementation.
    @Override
    public void bind(PropertyModel properties, ListItem item) {
        super.bind(properties, item);

        OfflineItem offlineItem = ((ListItem.OfflineItemListItem) item).item;

        mTitle.setText(offlineItem.title);
        mCaption.setText(UiUtils.generateGenericCaption(offlineItem));
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

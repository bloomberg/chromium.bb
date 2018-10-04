// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.holder;

import android.support.v7.widget.AppCompatImageButton;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.ListProperties;
import org.chromium.chrome.browser.download.home.list.UiUtils;
import org.chromium.chrome.browser.download.home.list.view.CircularProgressView;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.download.R;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;

/**
 * A {@link RecyclerView.ViewHolder} specifically meant to display an in-progress {@code
 * OfflineItem}.
 */
public class InProgressViewHolder extends ListItemViewHolder {
    private final TextView mTitle;
    private final TextView mCaption;
    private final CircularProgressView mActionButton;
    private final AppCompatImageButton mCancelButton;

    /**
     * Creates a new {@link InProgressViewHolder} instance.
     */
    public static InProgressViewHolder create(ViewGroup parent) {
        View view = LayoutInflater.from(parent.getContext())
                            .inflate(R.layout.download_manager_in_progress_item, null);
        return new InProgressViewHolder(view);
    }

    /** Constructor. */
    public InProgressViewHolder(View view) {
        super(view);

        mTitle = view.findViewById(R.id.title);
        mCaption = view.findViewById(R.id.caption);
        mActionButton = view.findViewById(R.id.action_button);
        mCancelButton = view.findViewById(R.id.cancel_button);
    }

    // ListItemViewHolder implementation.
    @Override
    public void bind(PropertyModel properties, ListItem item) {
        OfflineItem offlineItem = ((ListItem.OfflineItemListItem) item).item;

        mTitle.setText(offlineItem.title);
        mCaption.setText(UiUtils.generateInProgressLongCaption(offlineItem));

        mCancelButton.setOnClickListener(
                v -> properties.get(ListProperties.CALLBACK_CANCEL).onResult(offlineItem));

        UiUtils.setProgressForOfflineItem(mActionButton, offlineItem);
        mActionButton.setOnClickListener(view -> {
            switch (offlineItem.state) {
                case OfflineItemState.IN_PROGRESS: // Intentional fallthrough.
                case OfflineItemState.PENDING:
                    properties.get(ListProperties.CALLBACK_PAUSE).onResult(offlineItem);
                    break;
                case OfflineItemState.INTERRUPTED: // Intentional fallthrough.
                case OfflineItemState.PAUSED:
                    properties.get(ListProperties.CALLBACK_RESUME).onResult(offlineItem);
                    break;
                case OfflineItemState.COMPLETE: // Intentional fallthrough.
                case OfflineItemState.CANCELLED: // Intentional fallthrough.
                case OfflineItemState.FAILED:
                    // TODO(883387): Support retrying failed downloads.
                    properties.get(ListProperties.CALLBACK_RESUME).onResult(offlineItem);
                    break;
            }
        });
    }
}

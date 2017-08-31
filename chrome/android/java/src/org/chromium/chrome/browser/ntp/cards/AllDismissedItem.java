// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.support.annotation.StringRes;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.util.FeatureUtilities;

import java.util.Calendar;

/**
 * Displayed when all suggested content and their sections have been dismissed. Provides a button
 * to restore the dismissed sections and load new suggestions from the server.
 */
public class AllDismissedItem extends OptionalLeaf {

    @Override
    @ItemViewType
    public int getItemViewType() {
        return ItemViewType.ALL_DISMISSED;
    }

    @Override
    public void onBindViewHolder(NewTabPageViewHolder holder) {
        ((ViewHolder) holder).onBindViewHolder();
    }

    @Override
    public void visitOptionalItem(NodeVisitor visitor) {
        visitor.visitAllDismissedItem();
    }

    public void setVisible(boolean visible) {
        setVisibilityInternal(visible);
    }

    /**
     * ViewHolder for an item of type {@link ItemViewType#ALL_DISMISSED}.
     */
    public static class ViewHolder extends NewTabPageViewHolder {
        private final TextView mBodyTextView;

        public ViewHolder(ViewGroup root, final SectionList sections) {
            super(LayoutInflater.from(root.getContext())
                            .inflate(R.layout.new_tab_page_all_dismissed, root, false));
            mBodyTextView = itemView.findViewById(R.id.body_text);

            Button refreshButton = itemView.findViewById(R.id.action_button);
            ImageView backgroundView = itemView.findViewById(R.id.image);
            if (FeatureUtilities.isChromeHomeModernEnabled()) {
                ((ViewGroup) itemView).removeView(refreshButton);

                // Hide the view instead of removing it, because it is used to layout subsequent
                // views.
                itemView.findViewById(R.id.title_text).setVisibility(View.GONE);
                backgroundView.setImageResource(R.drawable.ntp_all_dismissed_white);
            } else {
                refreshButton.setOnClickListener(v -> {
                    NewTabPageUma.recordAction(NewTabPageUma.ACTION_CLICKED_ALL_DISMISSED_REFRESH);
                    sections.restoreDismissedSections();
                });
                backgroundView.setImageResource(R.drawable.ntp_all_dismissed_gray);
            }
        }

        public void onBindViewHolder() {
            int hour = Calendar.getInstance().get(Calendar.HOUR_OF_DAY);
            @StringRes
            final int messageId;
            if (FeatureUtilities.isChromeHomeModernEnabled()) {
                messageId = R.string.ntp_all_dismissed_body_text_modern;
            } else if (hour >= 0 && hour < 12) {
                messageId = R.string.ntp_all_dismissed_body_text_morning;
            } else if (hour >= 12 && hour < 17) {
                messageId = R.string.ntp_all_dismissed_body_text_afternoon;
            } else {
                messageId = R.string.ntp_all_dismissed_body_text_evening;
            }
            mBodyTextView.setText(messageId);
        }
    }
}

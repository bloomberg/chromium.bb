// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter.chips;

import android.content.res.ColorStateList;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.modelutil.RecyclerViewAdapter.ViewBinder;
import org.chromium.chrome.browser.modelutil.SimpleListObservable;
import org.chromium.chrome.browser.widget.TintedImageView;

/** Responsible for binding the {@link ChipsModel} to {@link ViewHolder}s in the RecyclerView. */
class ChipsViewBinder
        implements ViewBinder<SimpleListObservable<Chip>, ChipsViewBinder.ChipsViewHolder> {
    /** The {@link ViewHolder} responsible for reflecting a {@link Chip} to a {@link View}. */
    public static class ChipsViewHolder extends ViewHolder {
        private final int mTextStartPaddingWithIconPx;
        private final int mTextStartPaddingWithNoIconPx;

        private final TextView mText;
        private final TintedImageView mImage;

        /** Builds a ChipsViewHolder around a specific {@link View}. */
        public ChipsViewHolder(View itemView) {
            super(itemView);

            mText = (TextView) itemView.findViewById(R.id.text);
            mImage = (TintedImageView) itemView.findViewById(R.id.icon);

            ColorStateList textColors = mText.getTextColors();
            if (textColors != null) mImage.setTint(textColors);

            mTextStartPaddingWithIconPx =
                    mText.getResources().getDimensionPixelSize(R.dimen.chip_icon_padding);
            mTextStartPaddingWithNoIconPx =
                    mText.getResources().getDimensionPixelSize(R.dimen.chip_no_icon_padding);
        }

        /**
         * Pushes the properties of {@code chip} to {@code itemView}.
         * @param chip The {@link Chip} to visually reflect in the stored {@link View}.
         */
        public void bind(Chip chip) {
            itemView.setEnabled(chip.enabled);
            mText.setEnabled(chip.enabled);
            mImage.setEnabled(chip.enabled);

            itemView.setSelected(chip.selected);
            itemView.setOnClickListener(v -> chip.chipSelectedListener.run());
            mText.setContentDescription(mText.getContext().getText(chip.contentDescription));
            mText.setText(chip.text);

            int textStartPadding = mTextStartPaddingWithIconPx;
            if (chip.icon == Chip.INVALID_ICON_ID) {
                mImage.setVisibility(ViewGroup.GONE);
                textStartPadding = mTextStartPaddingWithNoIconPx;
            } else {
                textStartPadding = mTextStartPaddingWithIconPx;
                mImage.setVisibility(ViewGroup.VISIBLE);
                mImage.setImageResource(chip.icon);
            }

            ApiCompatibilityUtils.setPaddingRelative(mText, textStartPadding, mText.getPaddingTop(),
                    ApiCompatibilityUtils.getPaddingEnd(mText), mText.getPaddingBottom());
        }
    }

    // ViewBinder implementation.
    @Override
    public ChipsViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        return new ChipsViewHolder(
                LayoutInflater.from(parent.getContext()).inflate(R.layout.chip, null));
    }

    @Override
    public void onBindViewHolder(
            SimpleListObservable<Chip> model, ChipsViewHolder holder, int position) {
        holder.bind(model.get(position));
    }
}
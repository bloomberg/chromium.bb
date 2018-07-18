// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.view;

import android.content.Context;
import android.content.res.ColorStateList;
import android.support.graphics.drawable.AnimatedVectorDrawableCompat;
import android.support.v4.graphics.drawable.RoundedBitmapDrawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawableFactory;
import android.support.v7.content.res.AppCompatResources;
import android.util.AttributeSet;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.widget.TintedImageView;

/**
 * Represents a completed download in the download home.
 */
public class GenericListItemView extends ListItemView {
    private TintedImageView mThumbnail;
    private final ColorStateList mCheckedIconForegroundColorList;
    private final AnimatedVectorDrawableCompat mCheckDrawable;

    public GenericListItemView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mCheckDrawable = AnimatedVectorDrawableCompat.create(
                getContext(), R.drawable.ic_check_googblue_24dp_animated);
        mCheckedIconForegroundColorList = DownloadUtils.getIconForegroundColorList(context);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mThumbnail = (TintedImageView) findViewById(R.id.thumbnail);
    }

    @Override
    protected void updateView() {
        if (isChecked()) {
            mThumbnail.setBackgroundResource(R.drawable.list_item_icon_modern_bg);
            mThumbnail.getBackground().setLevel(
                    getResources().getInteger(R.integer.list_item_level_selected));

            mThumbnail.setImageDrawable(mCheckDrawable);
            mThumbnail.setTint(mCheckedIconForegroundColorList);
            mCheckDrawable.start();
        } else if (mThumbnailBitmap != null) {
            assert !mThumbnailBitmap.isRecycled();

            mThumbnail.setBackground(null);
            mThumbnail.setTint(null);

            RoundedBitmapDrawable drawable = RoundedBitmapDrawableFactory.create(
                    mThumbnail.getResources(), mThumbnailBitmap);
            drawable.setCircular(true);
            mThumbnail.setImageDrawable(drawable);
        } else {
            mThumbnail.setBackgroundResource(R.drawable.list_item_icon_modern_bg);
            mThumbnail.getBackground().setLevel(
                    mThumbnail.getResources().getInteger(R.integer.list_item_level_default));
            mThumbnail.setImageResource(mIconResId);
            mThumbnail.setTint(AppCompatResources.getColorStateList(
                    mThumbnail.getContext(), R.color.dark_mode_tint));
        }
    }
}

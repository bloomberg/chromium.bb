// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.view;

import android.content.Context;
import android.content.res.ColorStateList;
import android.support.graphics.drawable.AnimatedVectorDrawableCompat;
import android.util.AttributeSet;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.widget.TintedImageView;

/**
 * Represents an image in the download home and handles selection.
 */
public class ImageListItemView extends ListItemView {
    private final ColorStateList mCheckedIconForegroundColorList;
    private final AnimatedVectorDrawableCompat mCheckDrawable;
    private TintedImageView mSelectionImage;

    public ImageListItemView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mCheckDrawable = AnimatedVectorDrawableCompat.create(
                getContext(), R.drawable.ic_check_googblue_24dp_animated);
        mCheckedIconForegroundColorList = DownloadUtils.getIconForegroundColorList(context);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mSelectionImage = (TintedImageView) findViewById(R.id.selection);
    }

    @Override
    public void setChecked(boolean checked) {
        super.setChecked(checked);
        updateView();
    }

    @Override
    protected void updateView() {
        if (isChecked()) {
            mSelectionImage.setVisibility(VISIBLE);
            mSelectionImage.setBackgroundResource(R.drawable.list_item_icon_modern_bg);
            mSelectionImage.getBackground().setLevel(
                    getResources().getInteger(R.integer.list_item_level_selected));

            mSelectionImage.setImageDrawable(mCheckDrawable);
            mSelectionImage.setTint(mCheckedIconForegroundColorList);
            mCheckDrawable.start();
        } else if (isSelectionModeActive()) {
            mSelectionImage.setVisibility(VISIBLE);
            mSelectionImage.setBackground(null);
            mSelectionImage.setTint(null);

            mSelectionImage.setImageResource(R.drawable.download_circular_selector_transparent);
        } else {
            mSelectionImage.setBackground(null);
            mSelectionImage.setTint(null);
            mSelectionImage.setVisibility(GONE);
        }
    }
}

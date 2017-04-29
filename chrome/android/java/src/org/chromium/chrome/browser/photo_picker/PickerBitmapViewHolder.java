// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

import android.graphics.Bitmap;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.text.TextUtils;

import java.util.List;

/**
 * Holds on to a {@link PickerBitmapView} that displays information about a picker bitmap.
 */
public class PickerBitmapViewHolder
        extends ViewHolder implements DecoderServiceHost.ImageDecodedCallback {
    // Our parent category.
    private PickerCategoryView mCategoryView;

    // The bitmap view we are holding on to.
    private final PickerBitmapView mItemView;

    // The request we are showing the bitmap for.
    private PickerBitmap mBitmapDetails;

    /**
     * The PickerBitmapViewHolder.
     * @param itemView The {@link PickerBitmapView} view for showing the image.
     */
    public PickerBitmapViewHolder(PickerBitmapView itemView) {
        super(itemView);
        mItemView = itemView;
    }

    // DecoderServiceHost.ImageDecodedCallback

    @Override
    public void imageDecodedCallback(String filePath, Bitmap bitmap) {
        if (bitmap == null || bitmap.getWidth() == 0 || bitmap.getHeight() == 0) {
            return;
        }

        if (!TextUtils.equals(mBitmapDetails.getFilePath(), filePath)) {
            return;
        }

        mItemView.setThumbnailBitmap(bitmap);
    }

    /**
     * Display a single item from |position| in the PickerCategoryView.
     * @param categoryView The PickerCategoryView to use to fetch the image.
     * @param position The position of the item to fetch.
     */
    public void displayItem(PickerCategoryView categoryView, int position) {
        mCategoryView = categoryView;

        List<PickerBitmap> pickerBitmaps = mCategoryView.getPickerBitmaps();
        mBitmapDetails = pickerBitmaps.get(position);

        if (mBitmapDetails.type() == PickerBitmap.CAMERA
                || mBitmapDetails.type() == PickerBitmap.GALLERY) {
            mItemView.initialize(mBitmapDetails, null, false);
            return;
        }

        // TODO(finnur): Use cached image, if available.

        mItemView.initialize(mBitmapDetails, null, true);

        int size = mCategoryView.getImageSize();
        mCategoryView.getDecoderServiceHost().decodeImage(mBitmapDetails.getFilePath(), size, this);
    }

    /**
     * Returns the file path of the current request.
     */
    public String getFilePath() {
        return mBitmapDetails == null ? null : mBitmapDetails.getFilePath();
    }
}

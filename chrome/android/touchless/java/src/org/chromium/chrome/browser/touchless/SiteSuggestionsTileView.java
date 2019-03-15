// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;

import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.chrome.browser.widget.tile.TileView;
import org.chromium.chrome.touchless.R;

/**
 * View for a category name and site tiles.
 */
public class SiteSuggestionsTileView extends TileView {
    private final int mIconSizePx;

    // Used to generate textual icons.
    private RoundedIconGenerator mIconGenerator;

    public SiteSuggestionsTileView(Context ctx, AttributeSet attrs) {
        super(ctx, attrs);
        mIconSizePx = getResources().getDimensionPixelSize(R.dimen.tile_view_icon_size);
    }

    public void initialize(RoundedIconGenerator generator) {
        mIconGenerator = generator;
    }

    /**
     * Updates the icon in the tile.
     *
     * @param iconImage Bitmap icon to update.
     * @param text Text to generate scrabble text from.
     */
    public void updateIcon(Bitmap iconImage, String text) {
        setIconDrawable(getDrawableForBitmap(iconImage, text));
    }

    /**
     * Generates a Drawable from the Bitmap version of an icon.
     *
     * @param image The Bitmap image. Can be null.
     * @param text The text to generate a scrabble text from if the bitmap is null.
     * @return Drawable encapsulating either the scrabble tile or the bitmap.
     */
    public Drawable getDrawableForBitmap(Bitmap image, String text) {
        if (image == null) {
            return new BitmapDrawable(getResources(), mIconGenerator.generateIconForText(text));
        }
        return ViewUtils.createRoundedBitmapDrawable(
                Bitmap.createScaledBitmap(image, mIconSizePx, mIconSizePx, false),
                ViewUtils.DEFAULT_FAVICON_CORNER_RADIUS);
    }
}

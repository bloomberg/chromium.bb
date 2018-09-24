// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.chrome.browser.widget.tile.TileWithTextView;

/**
 * View for a category name and site tiles.
 */
public class ExploreSitesTileView extends TileWithTextView {
    private static final int TITLE_LINES = 1;

    private final int mIconSizePx;
    private RoundedIconGenerator mIconGenerator;

    public ExploreSitesTileView(Context ctx, AttributeSet attrs) {
        super(ctx, attrs);
        mIconSizePx = getResources().getDimensionPixelSize(R.dimen.tile_view_icon_size);
    }

    public void initialize(ExploreSitesSite site) {
        super.initialize(site.getTitle(), site.getUrl(), /* showOfflineBadge = */ false,
                getDrawableForBitmap(site.getIcon(), site.getTitle()), TITLE_LINES,
                TileWithTextView.Style.MODERN);
    }

    public void updateIcon(Bitmap iconImage, String text) {
        setIconDrawable(getDrawableForBitmap(iconImage, text));
    }

    public Drawable getDrawableForBitmap(Bitmap image, String text) {
        if (image == null) {
            return new BitmapDrawable(getResources(), mIconGenerator.generateIconForText(text));
        }
        return ViewUtils.createRoundedBitmapDrawable(image, mIconSizePx / 2);
    }

    /**
     * Sets icon generator. This is to help prevent an instance of icon generator
     * being created for each tile.
     */
    void setIconGenerator(RoundedIconGenerator generator) {
        mIconGenerator = generator;
    }
}

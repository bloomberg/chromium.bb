// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.util.AttributeSet;
import android.widget.ImageButton;

import org.chromium.chrome.browser.widget.ImageViewTinter.ImageViewTinterOwner;

/**
 * Implementation of ImageButton that allows tinting the Drawable for all states.
 * For usage, see {@link ImageViewTinter}.
 */
public class TintedImageButton extends ImageButton implements ImageViewTinterOwner {
    private ImageViewTinter mTinter;

    public TintedImageButton(Context context) {
        this(context, null);
    }

    public TintedImageButton(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public TintedImageButton(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        mTinter = new ImageViewTinter(this, attrs, defStyle);
    }

    @Override
    public void drawableStateChanged() {
        super.drawableStateChanged();
        mTinter.drawableStateChanged();
    }

    @Override
    public void setTint(ColorStateList tintList) {
        mTinter.setTint(tintList);
    }

    @Override
    public void onDraw(Canvas canvas) {
        super.onDraw(canvas);
    }
}

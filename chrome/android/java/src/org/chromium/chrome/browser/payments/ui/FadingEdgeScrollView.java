// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import android.content.Context;
import android.graphics.Canvas;
import android.util.AttributeSet;
import android.widget.ScrollView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.FadingShadow;

/**
 * An extension of the ScrollView that supports edge shadows with alpha components.
 */
public class FadingEdgeScrollView extends ScrollView {
    private static final int SHADOW_COLOR = 0x11000000;

    private final FadingShadow mFadingShadow;

    public FadingEdgeScrollView(Context context, AttributeSet attrs) {
        super(context, attrs);

        mFadingShadow = new FadingShadow(SHADOW_COLOR);
        setFadingEdgeLength(getResources().getDimensionPixelSize(R.dimen.ntp_shadow_height));
    }

    @Override
    protected void dispatchDraw(Canvas canvas) {
        super.dispatchDraw(canvas);
        setVerticalFadingEdgeEnabled(true);
        float topShadowStrength = getTopFadingEdgeStrength();
        float bottomShadowStrength = getBottomFadingEdgeStrength();
        float shadowHeight = getVerticalFadingEdgeLength();
        setVerticalFadingEdgeEnabled(false);

        mFadingShadow.drawShadow(this, canvas, FadingShadow.POSITION_TOP,
                shadowHeight, topShadowStrength);
        mFadingShadow.drawShadow(this, canvas, FadingShadow.POSITION_BOTTOM,
                shadowHeight, bottomShadowStrength);
    }
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;

/**
 * A new-fangled most visited item. Displays the title of the page beneath a large icon. If a large
 * icon isn't available, displays a rounded rectangle with a single letter in its place.
 */
public class IconMostVisitedItemView extends FrameLayout {

    private static final int HIGHLIGHT_COLOR = 0x550099cc;

    private boolean mLastDrawnPressed;

    /**
     * Constructor for inflating from XML.
     */
    public IconMostVisitedItemView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Sets the title text.
     */
    public void setTitle(String title) {
        ((TextView) findViewById(R.id.most_visited_title)).setText(title);
    }

    /**
     * Sets the icon.
     */
    public void setIcon(Drawable icon) {
        ImageView iconView = (ImageView) findViewById(R.id.most_visited_icon);
        iconView.setImageDrawable(icon);
    }

    @Override
    public void setPressed(boolean pressed) {
        super.setPressed(pressed);
        if (isPressed() != mLastDrawnPressed) invalidate();
    }

    @Override
    protected void dispatchDraw(Canvas canvas) {
        super.dispatchDraw(canvas);

        // Draw highlight overlay over the child views when this view is pressed.
        if (isPressed()) {
            Paint highlightPaint = new Paint();
            highlightPaint.setColor(HIGHLIGHT_COLOR);
            highlightPaint.setAntiAlias(true);
            RectF highlightRect = new RectF(0, 0, getWidth(), getHeight());
            int cornerRadius = getResources().getDimensionPixelOffset(
                    R.dimen.most_visited_bg_corner_radius);
            canvas.drawRoundRect(highlightRect, cornerRadius, cornerRadius, highlightPaint);
        }
        mLastDrawnPressed = isPressed();
    }
}

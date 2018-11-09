// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.ui;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.RectF;
import android.support.annotation.ColorInt;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.GestureDetector;
import android.view.GestureDetector.SimpleOnGestureListener;
import android.view.MotionEvent;
import android.view.View;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;

/**
 * A view that filters out touch events, letting through only the touch events events that are
 * within a specified touchable area.
 *
 * <p>This view decides whether to forward touch events. It does not manipulate them. This works at
 * the level of low-level touch events (up, down, move, cancel), not gestures.
 *
 * <p>TODO(crbug.com/806868): Consider merging this view with the overlay.
 */
public class TouchEventFilter extends View implements ChromeFullscreenManager.FullscreenListener {
    /**
     * Complain after there's been {@link TAP_TRACKING_COUNT} taps within
     * {@link @TAP_TRACKING_DURATION_MS} in the unallowed area.
     */
    private static final int TAP_TRACKING_COUNT = 3;
    private static final long TAP_TRACKING_DURATION_MS = 15_000;

    /** A client of this view. */
    public interface Client {
        /** Called after a certain number of unexpected taps. */
        void onUnexpectedTaps();
    }

    private Client mClient;
    private ChromeFullscreenManager mFullscreenManager;
    private final List<RectF> mTouchableArea = new ArrayList<>();
    private final Paint mGrayOut;
    private final Paint mClear;

    /** Whether filtering is enabled. */
    private boolean mEnabled;

    /** Padding added between the element area and the grayed-out area. */
    private final float mPaddingPx;

    /** Size of the corner of the cleared-out areas. */
    private final float mCornerPx;

    /** A single RectF instance used for drawing, to avoid creating many instances when drawing. */
    private final RectF mDrawRect = new RectF();

    /** Detects gestures that happen outside of the allowed area. */
    private final GestureDetector mGestureDetector;

    /** Times, in millisecond, of unexpected taps detected outside of the allowed area. */
    private final List<Long> mUnexpectedTapTimes = new ArrayList<>();

    public TouchEventFilter(Context context) {
        this(context, null, 0);
    }

    public TouchEventFilter(Context context, AttributeSet attributeSet) {
        this(context, attributeSet, 0);
    }

    public TouchEventFilter(Context context, AttributeSet attributeSet, int defStyle) {
        super(context, attributeSet, defStyle);
        mGrayOut = new Paint(Paint.ANTI_ALIAS_FLAG);
        mGrayOut.setColor(
                ApiCompatibilityUtils.getColor(context.getResources(), R.color.black_alpha_65));
        mGrayOut.setStyle(Paint.Style.FILL);

        mPaddingPx = TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, 2, context.getResources().getDisplayMetrics());
        mCornerPx = TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, 8, context.getResources().getDisplayMetrics());
        // TODO(crbug.com/806868): Add support for XML attributes configuration.

        mClear = new Paint();
        mClear.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.CLEAR));

        // Detect gestures in the unexpected area.
        mGestureDetector = new GestureDetector(context, new SimpleOnGestureListener() {
            @Override
            public boolean onSingleTapUp(MotionEvent e) {
                onUnexpectedTap(e);
                return true;
            }
        });

        updateTouchableArea(false, Collections.emptyList());
    }

    /** Initializes dependencies. */
    public void init(Client client, ChromeFullscreenManager fullscreenManager) {
        mClient = client;
        mFullscreenManager = fullscreenManager;
        mFullscreenManager.addListener(this);
    }

    /** Sets the color to be used for unusable areas. */
    public void setGrayOutColor(@ColorInt int color) {
        mGrayOut.setColor(color);
    }

    /**
     * Defines the area of the visible viewport that can be used.
     *
     * @param enabled if {@code false}, the filter is fully disabled and invisible
     * @param rectangles rectangles defining the area that can be used, may be empty
     */
    public void updateTouchableArea(boolean enabled, List<RectF> rectangles) {
        mEnabled = enabled;
        setAlpha(mEnabled ? 1.0f : 0.0f);
        mTouchableArea.clear();
        mTouchableArea.addAll(rectangles);
        invalidate();
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        switch (event.getAction()) {
            case MotionEvent.ACTION_SCROLL:
                // Scrolling is always safe. Let it through.
                return false;

            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_MOVE:
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                if (!mEnabled) return false; // let everything through

                // Only let through events if they're meant for the touchable area of the screen.
                int yTop = getVisualViewportTop();
                int yBottom = getVisualViewportBottom();
                if (event.getY() < yTop || event.getY() > yBottom) {
                    return false; // Let it through. It's meant for the controls.
                }
                int height = yBottom - yTop;
                boolean allowed = isInTouchableArea(((float) event.getX()) / getWidth(),
                        ((float) event.getY() - yTop) / height);
                if (!allowed) {
                    mGestureDetector.onTouchEvent(event);
                    return true; // handled
                }
                return false; // let it through

            default:
                return super.dispatchTouchEvent(event);
        }
    }

    /** Returns the origin of the visual viewport in this view. */
    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (!mEnabled) {
            return;
        }
        canvas.drawPaint(mGrayOut);

        int width = canvas.getWidth();
        int yTop = getVisualViewportTop();
        if (yTop > 0) {
            canvas.drawRect(0, 0, width, yTop, mClear);
        }
        int yBottom = getVisualViewportBottom();
        if (yBottom > 0) {
            canvas.drawRect(0, yBottom, width, canvas.getHeight(), mClear);
        }

        int height = yBottom - yTop;
        for (RectF rect : mTouchableArea) {
            mDrawRect.left = rect.left * width - mPaddingPx;
            mDrawRect.top = yTop + rect.top * height - mPaddingPx;
            mDrawRect.right = rect.right * width + mPaddingPx;
            mDrawRect.bottom = yTop + rect.bottom * height + mPaddingPx;
            if (mDrawRect.left <= 0 && mDrawRect.right >= width) {
                // Rounded corners look strange in the case where the rectangle takes exactly the
                // width of the screen.
                canvas.drawRect(mDrawRect, mClear);
            } else {
                canvas.drawRoundRect(mDrawRect, mCornerPx, mCornerPx, mClear);
            }
        }
    }

    @Override
    public void onContentOffsetChanged(float offset) {
        invalidate();
    }

    @Override
    public void onControlsOffsetChanged(float topOffset, float bottomOffset, boolean needsAnimate) {
        invalidate();
    }

    @Override
    public void onToggleOverlayVideoMode(boolean enabled) {}

    @Override
    public void onBottomControlsHeightChanged(int bottomControlsHeight) {
        invalidate();
    }

    @Override
    public void onUpdateViewportSize() {
        invalidate();
    }

    /** Considers whether to let the client know about unexpected taps. */
    private void onUnexpectedTap(MotionEvent e) {
        long eventTimeMs = e.getEventTime();
        for (Iterator<Long> iter = mUnexpectedTapTimes.iterator(); iter.hasNext();) {
            Long timeMs = iter.next();
            if ((eventTimeMs - timeMs) >= TAP_TRACKING_DURATION_MS) {
                iter.remove();
            }
        }
        mUnexpectedTapTimes.add(eventTimeMs);
        if (mUnexpectedTapTimes.size() == TAP_TRACKING_COUNT && mClient != null) {
            mClient.onUnexpectedTaps();
            mUnexpectedTapTimes.clear();
        }
    }

    private boolean isInTouchableArea(float x, float y) {
        for (RectF rect : mTouchableArea) {
            if (rect.contains(x, y, x, y)) {
                return true;
            }
        }
        return false;
    }

    /** Top position within the view of the visual viewport. */
    private int getVisualViewportTop() {
        if (mFullscreenManager == null) {
            return 0;
        }
        return (int) mFullscreenManager.getContentOffset();
    }

    /** Bottom position within the view of the visual viewport. */
    private int getVisualViewportBottom() {
        int bottomBarHeight = 0;
        if (mFullscreenManager != null) {
            bottomBarHeight = (int) (mFullscreenManager.getBottomControlsHeight()
                    - mFullscreenManager.getBottomControlOffset());
        }
        return getHeight() - bottomBarHeight;
    }
}

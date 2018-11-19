// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.ui;

import android.annotation.SuppressLint;
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
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.WebContents;

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
 */
public class TouchEventFilter
        extends View implements ChromeFullscreenManager.FullscreenListener, GestureStateListener {
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

        /**
         * Scroll the browser view by the given distance.
         *
         * <p>Distances are floats between -1.0 and 1.0, with 1 corresponding to the size of the
         * visible viewport.
         */
        void scrollBy(float distanceXRatio, float distanceYRatio);

        /** Asks for an update of the touchable area. */
        void updateTouchableArea();
    }

    private Client mClient;
    private ChromeFullscreenManager mFullscreenManager;
    private final List<RectF> mTouchableArea = new ArrayList<>();
    private final Paint mGrayOut;
    private final Paint mClear;

    /** Whether a partial-screen overlay is enabled or not. Has precedence over {@link
     * @mFullOverlayEnabled}. */
    private boolean mPartialOverlayEnabled;

    /** Whether a full-screen overlay is enabled or not. Is overridden by {@link
     * @mPartialOverlayEnabled}.*/
    private boolean mFullOverlayEnabled;

    /** Padding added between the element area and the grayed-out area. */
    private final float mPaddingPx;

    /** Size of the corner of the cleared-out areas. */
    private final float mCornerPx;

    /** A single RectF instance used for drawing, to avoid creating many instances when drawing. */
    private final RectF mDrawRect = new RectF();

    /** Detects gestures that happen outside of the allowed area. */
    private final GestureDetector mNonBrowserGesture;

    /** True while a gesture handled by {@link #mNonBrowserGesture} is in progress. */
    private boolean mNonBrowserGestureInProgress;

    /** Times, in millisecond, of unexpected taps detected outside of the allowed area. */
    private final List<Long> mUnexpectedTapTimes = new ArrayList<>();

    /** True while the browser is scrolling. */
    private boolean mBrowserScrolling;

    /**
     * Scrolling offset to use while scrolling right after scrolling.
     *
     * <p>This value shifts the touchable area by that many pixels while scrolling.
     */
    private int mBrowserScrollOffsetY;

    /**
     * Offset reported at the beginning of a scroll.
     *
     * <p>This is used to interpret the offsets reported by subsequent calls to {@link
     * #onScrollOffsetOrExtentChanged} or {@link #onScrollEnded}.
     */
    private int mInitialBrowserScrollOffsetY;

    /**
     * Current offset that applies on mTouchableArea.
     *
     * <p>This value shifts the touchable area by that many pixels after the end of a scroll and
     * before the next update, which resets this value.
     */
    private int mOffsetY;

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

        // Handles gestures that happen outside of the allowed area.
        mNonBrowserGesture = new GestureDetector(context, new SimpleOnGestureListener() {

            @Override
            public boolean onSingleTapUp(MotionEvent e) {
                onUnexpectedTap(e);
                return true;
            }

            @Override
            public boolean onScroll(MotionEvent downEvent, MotionEvent moveEvent, float distanceX,
                    float distanceY) {
                if (mPartialOverlayEnabled)
                    mClient.scrollBy(distanceX / getWidth(), distanceY / getVisualViewportHeight());
                return true;
            }
        });

        setFullOverlay(false);
        setPartialOverlay(false, Collections.emptyList());
    }

    /** Initializes dependencies. */
    public void init(
            Client client, ChromeFullscreenManager fullscreenManager, WebContents webContents) {
        mClient = client;
        mFullscreenManager = fullscreenManager;
        mFullscreenManager.addListener(this);
        GestureListenerManager.fromWebContents(webContents).addListener(this);
    }

    /** Sets the color to be used for unusable areas. */
    public void setGrayOutColor(@ColorInt int color) {
        mGrayOut.setColor(color);
    }

    /**
     * Enables/disables a full screen overlay.
     *
     * If both a full and a partial screen overlay are set, the partial overlay has precedence.
     *
     * @param enabled if {@code false}, the full screen overlay is disabled
     */
    public void setFullOverlay(boolean enabled) {
        if (mFullOverlayEnabled != enabled) {
            mFullOverlayEnabled = enabled;

            // reset tap counter each time the full screen overlay is disabled.
            if (!mFullOverlayEnabled) mUnexpectedTapTimes.clear();
            updateVisibility();
            invalidate();
        }
    }

    /**
     * Enables/disables a partial screen overlay.
     *
     * If both a full and a partial screen overlay are set, the partial overlay has precedence.
     *
     * @param enabled if {@code false}, the partial overlay is disabled
     * @param rectangles rectangles defining the area that can be used, may be empty
     */
    public void setPartialOverlay(boolean enabled, List<RectF> rectangles) {
        if (mPartialOverlayEnabled != enabled || (enabled && !mTouchableArea.equals(rectangles))) {
            mPartialOverlayEnabled = enabled;

            clearTouchableArea();
            mTouchableArea.addAll(rectangles);
            updateVisibility();
            invalidate();
        }
    }

    private boolean isOverlayShown() {
        return mFullOverlayEnabled || mPartialOverlayEnabled;
    }

    private void updateVisibility() {
        setAlpha(isOverlayShown() ? 1.0f : 0.0f);
    }

    private void clearTouchableArea() {
        mTouchableArea.clear();
        mOffsetY = 0;
        mInitialBrowserScrollOffsetY += mBrowserScrollOffsetY;
        mBrowserScrollOffsetY = 0;
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        // Note that partial overlays have precedence over full overlays
        if (mPartialOverlayEnabled) return dispatchTouchEventWithPartialOverlay(event);
        if (mFullOverlayEnabled) return dispatchTouchEventWithFullOverlay(event);
        return false;
    }

    private boolean dispatchTouchEventWithPartialOverlay(MotionEvent event) {
        switch (event.getAction()) {
            case MotionEvent.ACTION_SCROLL:
                // Scrolling is always safe. Let it through.
                return false;
            case MotionEvent.ACTION_MOVE:
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                if (mNonBrowserGestureInProgress) {
                    // If a non-browser gesture, started with an ACTION_DOWN, is in progress, give
                    // it priority over everything else until the next ACTION_DOWN.
                    mNonBrowserGesture.onTouchEvent(event);
                    return true;
                }
                if (mBrowserScrolling) {
                    // Events sent to the browser triggered scrolling, which was reported to
                    // onScrollStarted. Direct all events to the browser until the next ACTION_DOWN
                    // to avoid interrupting that scroll.
                    return false;
                }
            // fallthrough

            case MotionEvent.ACTION_DOWN:
                // Only let through events if they're meant for the touchable area of the screen.
                int yTop = getVisualViewportTop();
                int yBottom = getVisualViewportBottom();
                if (event.getY() < yTop || event.getY() > yBottom) {
                    return false; // Let it through. It's meant for the controls.
                }
                int height = yBottom - yTop;
                boolean allowed = isInTouchableArea(((float) event.getX()) / getWidth(),
                        (((float) event.getY() - yTop + mBrowserScrollOffsetY + mOffsetY)
                                / height));
                if (!allowed) {
                    mNonBrowserGestureInProgress = mNonBrowserGesture.onTouchEvent(event);
                    return true; // handled
                }
                return false; // let it through

            default:
                return super.dispatchTouchEvent(event);
        }
    }

    private boolean dispatchTouchEventWithFullOverlay(MotionEvent event) {
        mNonBrowserGesture.onTouchEvent(event);
        return true;
    }

    /** Returns the origin of the visual viewport in this view. */
    @Override
    @SuppressLint("CanvasSize")
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (!isOverlayShown()) {
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
            mDrawRect.top =
                    yTop + rect.top * height - mPaddingPx - mBrowserScrollOffsetY - mOffsetY;
            mDrawRect.right = rect.right * width + mPaddingPx;
            mDrawRect.bottom =
                    yTop + rect.bottom * height + mPaddingPx - mBrowserScrollOffsetY - mOffsetY;
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

    /** Called at the beginning of a scroll gesture triggered by the browser. */
    @Override
    public void onScrollStarted(int scrollOffsetY, int scrollExtentY) {
        mBrowserScrolling = true;
        mInitialBrowserScrollOffsetY = scrollOffsetY;
        mBrowserScrollOffsetY = 0;
        invalidate();
    }

    /** Called during a scroll gesture triggered by the browser. */
    @Override
    public void onScrollOffsetOrExtentChanged(int scrollOffsetY, int scrollExtentY) {
        if (!mBrowserScrolling) {
            // onScrollOffsetOrExtentChanged will be called alone, without onScrollStarted during a
            // Javascript-initiated scroll.
            mClient.updateTouchableArea();
            return;
        }
        mBrowserScrollOffsetY = scrollOffsetY - mInitialBrowserScrollOffsetY;
        invalidate();
        mClient.updateTouchableArea();
    }

    /** Called at the end of a scroll gesture triggered by the browser. */
    @Override
    public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {
        if (!mBrowserScrolling) {
            return;
        }
        mOffsetY += (scrollOffsetY - mInitialBrowserScrollOffsetY);
        mBrowserScrollOffsetY = 0;
        mBrowserScrolling = false;
        invalidate();
        mClient.updateTouchableArea();
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

    /** Height of the visual viewport. */
    private int getVisualViewportHeight() {
        return getVisualViewportBottom() - getVisualViewportTop();
    }
}

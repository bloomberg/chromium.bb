// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.ui;

import android.content.Context;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;

/**
 * A view that filters out touch events, letting through only the touch events events that are
 * allowed by {@link AllowTouchEvent}.
 *
 * <p>This view decides whether to forward touch events. It does not manipulate them. This works at
 * the level of low-level touch events (up, down, move, cancel), not gestures.
 */
public class TouchEventFilter extends View {
    /** A client of this view which decides whether to allow touch events or not. */
    public interface Client {
        /**
         * Returns {@code true} if a touch event at these coordinates
         * should be allowed.
         *
         * <p>Coordinates are values between 0 and 1 relative to the size of the visible viewport.
         */
        boolean allowTouchEvent(float x, float y);
    }

    private Client mClient;
    private ChromeFullscreenManager mFullscreenManager;

    public TouchEventFilter(Context context) {
        this(context, null, 0);
    }

    public TouchEventFilter(Context context, AttributeSet attributeSet) {
        this(context, attributeSet, 0);
    }

    public TouchEventFilter(Context context, AttributeSet attributeSet, int defStyle) {
        super(context, attributeSet, defStyle);
        setAlpha(0.0f);
    }

    public void init(Client client, ChromeFullscreenManager fullscreenManager) {
        mClient = client;
        mFullscreenManager = fullscreenManager;
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
                // Only let through events if they're meant for the touchable area of the screen.
                int yOrigin = getVisualViewportTop();
                int height = getVisualViewportHeight();
                if (event.getY() < yOrigin || event.getY() > (yOrigin + height)) {
                    return false; // Let it through. It's meant for the controls.
                }
                return !mClient.allowTouchEvent(((float) event.getX()) / getWidth(),
                        ((float) event.getY() - yOrigin) / height);

            default:
                return super.dispatchTouchEvent(event);
        }
    }

    /** Returns the origin of the visual viewport in this view. */
    private int getVisualViewportTop() {
        if (mFullscreenManager == null) {
            return 0;
        }
        return (int) mFullscreenManager.getContentOffset();
    }

    /**
     * Returns the height of the visual viewport.
     *
     * This relies on the assumption that the size of this view, minus the controls, corresponds to
     * the size of the visible viewport, on the Javascript side.
     */
    private int getVisualViewportHeight() {
        if (mFullscreenManager == null) {
            return getHeight();
        }
        int height = getHeight() - getVisualViewportTop();
        height -= (int) (mFullscreenManager.getBottomControlsHeight()
                - mFullscreenManager.getBottomControlOffset());
        return height;
    }
}

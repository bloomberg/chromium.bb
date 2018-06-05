// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.Rect;
import android.os.Build;
import android.view.View;
import android.view.WindowInsets;

import org.chromium.base.ObserverList;

/**
 * The purpose of this view is to store the system window insets (OSK, status bar) for
 * later use.
 */
public class InsetObserverView extends View {
    private final Rect mWindowInsets;
    private final ObserverList<WindowInsetObserver> mObservers;

    /**
     * Allows observing changes to the window insets from Android system UI.
     */
    public interface WindowInsetObserver {
        /**
         * Triggered when the window insets have changed.
         *
         * @param left The left inset.
         * @param top The top inset.
         * @param right The right inset (but it feels so wrong).
         * @param bottom The bottom inset.
         */
        void onInsetChanged(int left, int top, int right, int bottom);
    }

    /**
     * Constructs a new {@link InsetObserverView} for the appropriate Android version.
     * @param context The Context the view is running in, through which it can access the current
     *            theme, resources, etc.
     * @return an instance of a InsetObserverView.
     */
    public static InsetObserverView create(Context context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            return new InsetObserverView(context);
        }
        return new InsetObserverViewApi21(context);
    }

    /**
     * Creates an instance of {@link InsetObserverView}.
     * @param context The Context to create this {@link InsetObserverView} in.
     */
    public InsetObserverView(Context context) {
        super(context);
        setVisibility(INVISIBLE);
        mWindowInsets = new Rect();
        mObservers = new ObserverList<>();
    }

    /**
     * Returns the left {@link WindowInsets} in pixels.
     */
    public int getSystemWindowInsetsLeft() {
        return mWindowInsets.left;
    }

    /**
     * Returns the top {@link WindowInsets} in pixels.
     */
    public int getSystemWindowInsetsTop() {
        return mWindowInsets.top;
    }

    /**
     * Returns the right {@link WindowInsets} in pixels.
     */
    public int getSystemWindowInsetsRight() {
        return mWindowInsets.right;
    }

    /**
     * Returns the bottom {@link WindowInsets} in pixels.
     */
    public int getSystemWindowInsetsBottom() {
        return mWindowInsets.bottom;
    }

    /**
     * Add an observer to be notified when the window insets have changed.
     */
    public void addObserver(WindowInsetObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Remove an observer of window inset changes.
     */
    public void removeObserver(WindowInsetObserver observer) {
        mObservers.removeObserver(observer);
    }

    @SuppressWarnings("deprecation")
    @Override
    protected boolean fitSystemWindows(Rect insets) {
        // For Lollipop and above, onApplyWindowInsets will set the insets.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            onInsetChanged(insets.left, insets.top, insets.right, insets.bottom);
        }
        return false;
    }

    /**
     * Updates the window insets and notifies all observers if the values did indeed change.
     *
     * @param left The updated left inset.
     * @param top The updated right inset.
     * @param right The updated right inset.
     * @param bottom The updated bottom inset.
     */
    protected void onInsetChanged(int left, int top, int right, int bottom) {
        if (mWindowInsets.left == left && mWindowInsets.top == top && mWindowInsets.right == right
                && mWindowInsets.bottom == bottom) {
            return;
        }

        mWindowInsets.set(left, top, right, bottom);

        for (WindowInsetObserver observer : mObservers) {
            observer.onInsetChanged(left, top, right, bottom);
        }
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private static class InsetObserverViewApi21 extends InsetObserverView {
        /**
         * Creates an instance of {@link InsetObserverView} for Android versions L and above.
         * @param context The Context to create this {@link InsetObserverView} in.
         */
        InsetObserverViewApi21(Context context) {
            super(context);
        }

        @Override
        public WindowInsets onApplyWindowInsets(WindowInsets insets) {
            onInsetChanged(insets.getSystemWindowInsetLeft(), insets.getSystemWindowInsetTop(),
                    insets.getSystemWindowInsetRight(), insets.getSystemWindowInsetBottom());
            return insets;
        }
    }
}

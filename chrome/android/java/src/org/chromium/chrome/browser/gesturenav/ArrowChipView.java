// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.content.Context;
import android.support.annotation.StyleRes;
import android.view.View;
import android.view.animation.Animation.AnimationListener;

import org.chromium.chrome.R;
import org.chromium.ui.widget.ChipView;

/**
 * A widget for arrow and an optional 'close chrome' indicator used for overscroll navigation.
 * TODO(jinsukkim): Implement this using a custom view.
 */
public final class ArrowChipView extends ChipView {
    private AnimationListener mListener;

    public ArrowChipView(Context context, @StyleRes int style) {
        super(context, style);
        getPrimaryTextView().setText(context.getResources().getString(
                R.string.overscroll_navigation_close_chrome, context.getString(R.string.app_name)));
        getPrimaryTextView().setVisibility(View.GONE);
    }

    /**
     * Sets {@link AnimationListener} used when the widget disappears at the end of
     * user gesture.
     * @param listener Listener object.
     */
    public void setAnimationListener(AnimationListener listener) {
        mListener = listener;
    }

    /**
     * @return {@code true} if the widget is showing the close chrome indicator text.
     */
    public boolean isShowingCaption() {
        return getPrimaryTextView().getVisibility() == View.VISIBLE;
    }

    /**
     * Shows or hides the close chrome indicator.
     * @param on {@code true} if the indicator should appear.
     */
    public void showCaption(boolean on) {
        if (on && !isShowingCaption()) {
            getPrimaryTextView().setVisibility(View.VISIBLE);
            // Measure the width again after the indicator text becomes visible.
            measure(0, 0);
        } else if (!on && isShowingCaption()) {
            getPrimaryTextView().setVisibility(View.GONE);
        }
    }

    @Override
    public void onAnimationStart() {
        super.onAnimationStart();
        if (mListener != null) {
            mListener.onAnimationStart(getAnimation());
        }
    }

    @Override
    public void onAnimationEnd() {
        super.onAnimationEnd();
        if (mListener != null) {
            mListener.onAnimationEnd(getAnimation());
        }
    }
}

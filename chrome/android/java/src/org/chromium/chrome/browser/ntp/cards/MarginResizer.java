// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.DisplayStyleObserver;
import org.chromium.chrome.browser.ntp.UiConfig;

/**
 * Adds lateral margins to the view when the display style is {@link UiConfig#DISPLAY_STYLE_WIDE}.
 */
public class MarginResizer implements DisplayStyleObserver {
    private final int mDefaultMarginSizePixels;
    private final int mWideMarginSizePixels;
    private final View mView;

    /**
     * Factory method that creates a {@link MarginResizer} and wraps it in a
     * {@link DisplayStyleObserverAdapter} that will take care of invoking it when appropriate.
     * Uses 0px as margins when on a non wide layout.
     * @param view the view that will have its margins resized
     * @param config the UiConfig object to subscribe to
     */
    public static DisplayStyleObserverAdapter createWithViewAdapter(View view, UiConfig config) {
        return new DisplayStyleObserverAdapter(view, config, new MarginResizer(view, 0));
    }

    /**
     * Factory method that creates a {@link MarginResizer} and wraps it in a
     * {@link DisplayStyleObserverAdapter} that will take care of invoking it when appropriate
     * @param view the view that will have its margins resized
     * @param config the UiConfig object to subscribe to
     * @param defaultMargin  the margins to use on non-wide layouts
     */
    public static DisplayStyleObserverAdapter createWithViewAdapter(
            View view, UiConfig config, int defaultMargin) {
        return new DisplayStyleObserverAdapter(
                view, config, new MarginResizer(view, defaultMargin));
    }

    public MarginResizer(View view, int defaultMarginSize) {
        mView = view;
        mDefaultMarginSizePixels = defaultMarginSize;
        mWideMarginSizePixels =
                view.getResources().getDimensionPixelSize(R.dimen.ntp_wide_card_lateral_margins);
    }

    @Override
    public void onDisplayStyleChanged(@UiConfig.DisplayStyle int newDisplayStyle) {
        MarginLayoutParams layoutParams = (MarginLayoutParams) mView.getLayoutParams();
        if (newDisplayStyle == UiConfig.DISPLAY_STYLE_WIDE) {
            layoutParams.setMargins(mWideMarginSizePixels, layoutParams.topMargin,
                    mWideMarginSizePixels, layoutParams.bottomMargin);
        } else {
            layoutParams.setMargins(mDefaultMarginSizePixels, layoutParams.topMargin,
                    mDefaultMarginSizePixels, layoutParams.bottomMargin);
        }
    }
}

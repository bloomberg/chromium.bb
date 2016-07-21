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
    private final int mWideMarginSizePixels;
    private final View mView;

    public static DisplayStyleObserverAdapter createWithViewAdapter(View view, UiConfig config) {
        return new DisplayStyleObserverAdapter(view, config, new MarginResizer(view));
    }

    public MarginResizer(View view) {
        mView = view;
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
            layoutParams.setMargins(0, layoutParams.topMargin, 0, layoutParams.bottomMargin);
        }
    }
}

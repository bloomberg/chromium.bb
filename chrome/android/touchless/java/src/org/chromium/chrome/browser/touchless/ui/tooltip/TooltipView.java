// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless.ui.tooltip;

import android.content.Context;
import android.support.design.widget.CoordinatorLayout;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.chrome.touchless.R;

/**
 * Responsible for displaying tooltips in NoTouch mode.
 */
public class TooltipView extends FrameLayout {
    private ViewGroup mContainerView;

    public TooltipView(Context context) {
        super(context);
        init();
    }

    public TooltipView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public TooltipView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    private void init() {
        CoordinatorLayout.LayoutParams layoutParams =
                new CoordinatorLayout.LayoutParams(CoordinatorLayout.LayoutParams.WRAP_CONTENT,
                        CoordinatorLayout.LayoutParams.WRAP_CONTENT);
        layoutParams.gravity = Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL;
        layoutParams.bottomMargin =
                (int) getResources().getDimension(R.dimen.notouch_tooltip_margin_bottom);
        setLayoutParams(layoutParams);
        mContainerView = (ViewGroup) inflate(getContext(), R.layout.notouch_tooltip_view, null);
        mContainerView.setVisibility(GONE);
        addView(mContainerView);
    }

    public void show(View view) {
        mContainerView.removeAllViews();
        mContainerView.addView(view);
        mContainerView.setVisibility(VISIBLE);
    }

    public void hide() {
        mContainerView.setVisibility(GONE);
    }
}

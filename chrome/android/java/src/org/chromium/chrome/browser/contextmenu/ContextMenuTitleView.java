// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.content.Context;
import android.widget.ScrollView;
import android.widget.TextView;

import org.chromium.chrome.R;

/**
 * Context menu title text view that is restricted height and scrollable.
 */
public class ContextMenuTitleView extends ScrollView {
    private static final int MAX_HEIGHT_DP = 70;
    private static final int PADDING_DP = 16;

    private final float mDpToPx;

    /**
     * @param context Context to be used to inflate this view.
     * @param title String to be displayed as the title.
     */
    public ContextMenuTitleView(Context context, String title) {
        super(context);
        mDpToPx = getContext().getResources().getDisplayMetrics().density;
        int padding = (int) (PADDING_DP * mDpToPx);
        setPadding(padding, padding, padding, 0);

        TextView titleView = new TextView(context);
        titleView.setText(title);
        titleView.setTextColor(getResources().getColor(R.color.default_text_color));
        titleView.setPadding(0, 0, 0, padding);
        addView(titleView);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int maxHeight = (int) (MAX_HEIGHT_DP * mDpToPx);
        heightMeasureSpec = MeasureSpec.makeMeasureSpec(maxHeight, MeasureSpec.AT_MOST);
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }
}
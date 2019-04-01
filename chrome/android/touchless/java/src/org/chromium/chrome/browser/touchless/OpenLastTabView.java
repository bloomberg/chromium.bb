// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.touchless.R;
import org.chromium.ui.widget.ChipView;

/**
 * View for the button to open the last tab.
 */
public class OpenLastTabView extends FrameLayout {
    private LinearLayout mPlaceholder;
    private ChipView mLastTabChip;

    public OpenLastTabView(Context context, AttributeSet attrs) {
        super(context, attrs);

        LayoutInflater.from(context).inflate(R.layout.open_last_tab_button, this);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mPlaceholder = findViewById(R.id.placeholder);
        mLastTabChip = findViewById(R.id.last_tab_chip);
        // Allow the system ui to control our background.
        mLastTabChip.setBackground(null);

        TextView primaryTextView = mLastTabChip.getPrimaryTextView();
        TextView secondaryTextView = mLastTabChip.getSecondaryTextView();

        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        primaryTextView.setLayoutParams(params);
        primaryTextView.setSingleLine(true);
        primaryTextView.setEllipsize(TextUtils.TruncateAt.END);
        primaryTextView.setPadding((int) getContext().getResources().getDimension(
                                           R.dimen.open_last_tab_primary_text_margin_left),
                0, 0, 0);
        ApiCompatibilityUtils.setTextAppearance(
                primaryTextView, R.style.TextAppearance_BlackTitle2);

        params = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        secondaryTextView.setLayoutParams(params);
        secondaryTextView.setMinWidth(getContext().getResources().getDimensionPixelSize(
                R.dimen.open_last_tab_timestamp_min_width));
        secondaryTextView.setTextAlignment(TEXT_ALIGNMENT_VIEW_END);
        ApiCompatibilityUtils.setTextAppearance(
                secondaryTextView, R.style.TextAppearance_BlackCaption);
    }

    void setLoadSuccess(boolean loadSuccess) {
        this.setVisibility(View.VISIBLE);

        if (loadSuccess) {
            mPlaceholder.setVisibility(View.GONE);
            mLastTabChip.setVisibility(View.VISIBLE);
        } else {
            mLastTabChip.setVisibility(View.GONE);
            mPlaceholder.setVisibility(View.VISIBLE);
        }
    }

    void setOpenLastTabOnClickListener(OnClickListener onClickListener) {
        mLastTabChip.setOnClickListener(onClickListener);
    }

    void setFavicon(Bitmap favicon) {
        mLastTabChip.setIcon(new BitmapDrawable(getContext().getResources(), favicon), false);
    }

    void setTitle(String title) {
        mLastTabChip.getPrimaryTextView().setText(title);
    }

    void setTimestamp(String timestamp) {
        mLastTabChip.getSecondaryTextView().setText(timestamp);
    }
}

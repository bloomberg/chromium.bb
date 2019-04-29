// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.Callback;
import org.chromium.chrome.touchless.R;

/**
 * View for the button to open the last tab.
 */
// TODO(crbug.com/948858): Add render tests for this view.
public class OpenLastTabView extends FrameLayout {
    private LinearLayout mPlaceholder;
    private TextView mPlaceholderText;

    private LinearLayout mLastTabView;
    private ImageView mIconView;
    private TextView mTitleText;
    private TextView mTimestampText;
    private Callback<View> mAsyncFocusDelegate;

    public OpenLastTabView(Context context, AttributeSet attrs) {
        super(context, attrs);

        LayoutInflater.from(context).inflate(R.layout.open_last_tab_button, this);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mPlaceholder = findViewById(R.id.placeholder);
        mPlaceholderText = findViewById(R.id.placeholder_text);

        mLastTabView = findViewById(R.id.open_last_tab);
        mIconView = findViewById(R.id.favicon);
        mTitleText = findViewById(R.id.title);
        mTimestampText = findViewById(R.id.timestamp);
    }

    void setLoadSuccess(boolean loadSuccess) {
        this.setVisibility(View.VISIBLE);

        if (loadSuccess) {
            mPlaceholder.setVisibility(View.GONE);
            mLastTabView.setVisibility(View.VISIBLE);
        } else {
            mLastTabView.setVisibility(View.GONE);
            mPlaceholder.setVisibility(View.VISIBLE);
        }
    }

    void setFirstLaunched(boolean firstLaunched) {
        if (firstLaunched) {
            mPlaceholderText.setText(R.string.open_last_tab_placeholder_first_launch);
        }
    }

    void setOpenLastTabOnClickListener(OnClickListener onClickListener) {
        mLastTabView.setOnClickListener(onClickListener);
    }

    void setFavicon(Bitmap favicon) {
        mIconView.setImageDrawable(new BitmapDrawable(getContext().getResources(), favicon));
    }

    void setTitle(String title) {
        mTitleText.setText(title);
    }

    void setTimestamp(String timestamp) {
        mTimestampText.setText(timestamp);
    }

    void setOnFocusCallback(Runnable callback) {
        mLastTabView.setOnFocusChangeListener((View view, boolean hasFocus) -> {
            if (hasFocus) {
                callback.run();
            }
        });
    }

    void triggerRequestFocus() {
        if (mAsyncFocusDelegate != null) {
            mAsyncFocusDelegate.onResult(mLastTabView);
        }
    }

    void setAsyncFocusDelegate(Callback<View> asyncFocusDelegate) {
        mAsyncFocusDelegate = asyncFocusDelegate;
    }
}

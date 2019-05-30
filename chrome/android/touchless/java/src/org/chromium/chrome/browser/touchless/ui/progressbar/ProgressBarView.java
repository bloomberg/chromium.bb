// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless.ui.progressbar;

import android.content.Context;
import android.support.design.widget.CoordinatorLayout;
import android.util.AttributeSet;
import android.view.Gravity;
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.chromium.chrome.touchless.R;

/**
 * View responsible for displaying touchless progress bar UI. It displays the progress and current
 * eTLD+1.
 */
public class ProgressBarView extends RelativeLayout {
    private TextView mUrlTextView;
    private PillProgressBarView mProgressBar;

    public ProgressBarView(Context context) {
        super(context);
        init();
    }

    public ProgressBarView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public ProgressBarView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    private void init() {
        CoordinatorLayout.LayoutParams layoutParams =
                new CoordinatorLayout.LayoutParams(CoordinatorLayout.LayoutParams.WRAP_CONTENT,
                        CoordinatorLayout.LayoutParams.WRAP_CONTENT);
        layoutParams.gravity = Gravity.TOP | Gravity.CENTER_HORIZONTAL;
        setLayoutParams(layoutParams);

        inflate(getContext(), R.layout.notouch_progress_bar_view, this);
        mProgressBar = findViewById(R.id.notouch_progress_bar_view);
        mUrlTextView = findViewById(R.id.notouch_url_text_view);

        setVisibility(false);
    }

    void setProgress(float progressFraction) {
        assert progressFraction >= 0f && progressFraction <= 1f;
        mProgressBar.setProgress((int) (progressFraction * mProgressBar.getMax()));
    }

    void setUrlText(String text) {
        mUrlTextView.setText(text);
    }

    void setVisibility(boolean isVisible) {
        setVisibility(isVisible ? VISIBLE : GONE);
    }
}

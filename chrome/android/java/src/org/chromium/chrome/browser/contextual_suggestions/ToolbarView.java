// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.R;

/** The toolbar view, containing an icon, title and close button. */
public class ToolbarView extends LinearLayout {
    private View mCloseButton;
    private TextView mTitle;

    public ToolbarView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mCloseButton = findViewById(R.id.close_button);
        mTitle = (TextView) findViewById(R.id.title);
    }

    void setCloseButtonOnClickListener(OnClickListener listener) {
        mCloseButton.setOnClickListener(listener);
    }

    void setTitle(String title) {
        mTitle.setText(title);
    }
}

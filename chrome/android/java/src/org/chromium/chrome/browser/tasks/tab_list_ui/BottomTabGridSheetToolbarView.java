// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_list_ui;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.ListMenuButton;

/**
 * Represents the toolbar in bottom tab grid sheet.
 * {@link BottomTabGridSheetToolbarCoordinator}
 */
public class BottomTabGridSheetToolbarView extends FrameLayout {
    private ListMenuButton mCollapseButton;
    private ListMenuButton mAddButton;
    private TextView mTitle;

    public BottomTabGridSheetToolbarView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mAddButton = findViewById(R.id.add);
        mCollapseButton = findViewById(R.id.collapse);
        mTitle = (TextView) findViewById(R.id.title);
    }

    void setCollapseButtonOnClickListener(OnClickListener listener) {
        mCollapseButton.setOnClickListener(listener);
    }

    void setAddButtonOnClickListener(OnClickListener listener) {
        mAddButton.setOnClickListener(listener);
    }

    void setTitle(String title) {
        mTitle.setText(title);
    }
}

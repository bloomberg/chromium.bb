// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.selection.SelectableItemView;

/**
 * The SelectableItemView for items displayed in the browsing history UI.
 */
public class HistoryItemView extends SelectableItemView<HistoryItem> {
    private TextView mTitle;
    private TextView mDomain;

    public HistoryItemView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitle = (TextView) findViewById(R.id.title);
        mDomain = (TextView) findViewById(R.id.domain);
    }

    @Override
    public void setItem(HistoryItem item) {
        super.setItem(item);
        mTitle.setText(item.getTitle());
        mDomain.setText(item.getDomain());
    }

    @Override
    protected void onClick() {
        // TODO(twellington): Handle clicks on the HistoryItemView.
    }
}

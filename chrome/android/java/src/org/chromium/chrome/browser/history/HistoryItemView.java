// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.TintedImageButton;
import org.chromium.chrome.browser.widget.selection.SelectableItemView;

/**
 * The SelectableItemView for items displayed in the browsing history UI.
 */
public class HistoryItemView extends SelectableItemView<HistoryItem> {
    private TextView mTitle;
    private TextView mDomain;
    private TintedImageButton mRemoveButton;

    public HistoryItemView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitle = (TextView) findViewById(R.id.title);
        mDomain = (TextView) findViewById(R.id.domain);
        mRemoveButton = (TintedImageButton) findViewById(R.id.remove);
        mRemoveButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                remove();
            }
        });
    }

    @Override
    public void setItem(HistoryItem item) {
        super.setItem(item);
        mTitle.setText(item.getTitle());
        mDomain.setText(item.getDomain());
    }

    /**
     * @param manager The HistoryManager associated with this item.
     */
    public void setHistoryManager(HistoryManager manager) {
        getItem().setHistoryManager(manager);
    }

    /**
     * Removes the item associated with this view.
     */
    public void remove() {
        getItem().remove();
    }

    @Override
    protected void onClick() {
        if (getItem() != null) getItem().open();
    }
}

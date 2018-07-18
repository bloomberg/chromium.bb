// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.view;

import android.content.Context;
import android.graphics.Bitmap;
import android.util.AttributeSet;

import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.widget.selection.SelectableItemViewBase;

/** A class that represents a variety of possible list items to show in downloads home. */
public abstract class ListItemView extends SelectableItemViewBase<ListItem> {
    private Runnable mClickCallback;
    protected Bitmap mThumbnailBitmap;
    protected int mIconResId;

    public ListItemView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public void setClickCallback(Runnable listener) {
        mClickCallback = listener;
    }

    @Override
    protected void onClick() {
        mClickCallback.run();
    }

    public void setThumbnail(Bitmap thumbnail) {
        mThumbnailBitmap = thumbnail;
        updateView();
    }

    public void setThumbnailResource(int iconResId) {
        mIconResId = iconResId;
        updateView();
    }
}
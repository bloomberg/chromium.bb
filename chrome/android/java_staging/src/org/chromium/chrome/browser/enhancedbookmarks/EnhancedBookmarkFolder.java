// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enhancedbookmarks;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.enhancedbookmarks.EnhancedBookmarkItemsAdapter.BookmarkGrid;
import org.chromium.chrome.browser.widget.CustomShapeDrawable.DarkBackgroundCircularDrawable;
import org.chromium.chrome.browser.widget.TintedDrawable;
import org.chromium.components.bookmarks.BookmarkId;

/**
 * A view that shows a bookmark folder's title, in the enhanced bookmarks UI.
 */
abstract class EnhancedBookmarkFolder extends FrameLayout implements BookmarkGrid {

    @SuppressLint("Instantiatable")
    static class EnhancedBookmarkListFolder extends EnhancedBookmarkFolder {
        public EnhancedBookmarkListFolder(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        @Override
        protected void setupIcon() {
            DarkBackgroundCircularDrawable icon = new DarkBackgroundCircularDrawable(
                    BitmapFactory.decodeResource(getResources(), R.drawable.eb_folder));
            int size = getResources().getDimensionPixelSize(
                    R.dimen.enhanced_bookmark_folder_item_icon_size);
            icon.setBounds(0, 0, size, size);
            ApiCompatibilityUtils.setCompoundDrawablesRelative(mTitle, icon, null, null, null);
        }
    }

    @SuppressLint("Instantiatable")
    static class EnhancedBookmarkGridFolder extends EnhancedBookmarkFolder {
        public EnhancedBookmarkGridFolder(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        @Override
        protected void setupIcon() {
            BitmapDrawable icon = TintedDrawable.constructTintedDrawable(
                    getResources(), R.drawable.eb_folder);
            ApiCompatibilityUtils.setCompoundDrawablesRelativeWithIntrinsicBounds(
                    mTitle, icon, null, null, null);
        }
    }

    private BookmarkId mBookmarkId;
    private EnhancedBookmarkItemHighlightView mHighlightView;
    private EnhancedBookmarkDelegate mDelegate;
    protected TextView mTitle;

    /**
     * Constructor for inflating from XML.
     */
    public EnhancedBookmarkFolder(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mHighlightView = (EnhancedBookmarkItemHighlightView) findViewById(R.id.highlight);
        mTitle = (TextView) findViewById(R.id.title);
        setupIcon();
    }

    protected abstract void setupIcon();

    @Override
    public void setBookmarkId(BookmarkId bookmarkId) {
        mBookmarkId = bookmarkId;
        mTitle.setText(mDelegate.getModel().getBookmarkTitle(bookmarkId));
    }

    @Override
    public BookmarkId getBookmarkId() {
        return mBookmarkId;
    }

    /**
     * Sets the delegate the folder holds.
     */
    void setDelegate(EnhancedBookmarkDelegate delegate) {
        mDelegate = delegate;
    }

    @Override
    public boolean isChecked() {
        return mHighlightView.isChecked();
    }

    @Override
    public void toggle() {
        setChecked(!isChecked());
    }

    @Override
    public void setChecked(boolean checked) {
        mHighlightView.setChecked(checked);
    }

    @Override
    public void onClick(View v) {
        mDelegate.openFolder(mBookmarkId);
    }
}

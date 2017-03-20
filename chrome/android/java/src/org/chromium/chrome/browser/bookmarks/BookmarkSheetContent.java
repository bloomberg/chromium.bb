// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.support.v7.widget.Toolbar;
import android.view.View;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.toolbar.BottomToolbarPhone;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContentController;

/**
 * A {@link BottomSheetContent} holding a {@link BookmarkManager} for display in the BottomSheet.
 */
public class BookmarkSheetContent implements BottomSheetContent {
    private final View mContentView;
    private final Toolbar mToolbarView;
    private BookmarkManager mBookmarkManager;

    /**
     * @param activity The activity displaying the bookmark manager UI.
     */
    public BookmarkSheetContent(ChromeActivity activity) {
        mBookmarkManager = new BookmarkManager(activity, false);
        mBookmarkManager.updateForUrl(UrlConstants.BOOKMARKS_URL);
        mContentView = mBookmarkManager.getView();
        mToolbarView = mBookmarkManager.detachToolbarView();
        ((BottomToolbarPhone) activity.getToolbarManager().getToolbar())
                .setOtherToolbarStyle(mToolbarView);
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public View getToolbarView() {
        return mToolbarView;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mBookmarkManager.getVerticalScrollOffset();
    }

    @Override
    public void destroy() {
        mBookmarkManager.destroy();
        mBookmarkManager = null;
    }

    @Override
    public int getType() {
        return BottomSheetContentController.TYPE_BOOKMARKS;
    }
}

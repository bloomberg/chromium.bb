// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.app.Activity;
import android.view.View;

import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.widget.BottomSheet.BottomSheetContent;

/**
 * A {@link BottomSheetContent} holding a {@link BookmarkManager} for display in the BottomSheet.
 */
public class BookmarkSheetContent implements BottomSheetContent {
    private final View mContentView;
    private BookmarkManager mBookmarkManager;

    /**
     * @param activity The activity displaying the bookmark manager UI.
     */
    public BookmarkSheetContent(Activity activity) {
        mBookmarkManager = new BookmarkManager(activity, false);
        mBookmarkManager.updateForUrl(UrlConstants.BOOKMARKS_URL);
        mContentView = mBookmarkManager.detachContentView();
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public View getToolbarView() {
        return null;
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
}

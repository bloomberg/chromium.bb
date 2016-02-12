// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.WindowManager;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.components.bookmarks.BookmarkId;

/**
 * The activity that displays the bookmark UI on the phone. It keeps a {@link BookmarkManager}
 * inside of it and creates a snackbar manager. This activity should only be shown on phones; on
 * tablet the enhanced bookmark UI is shown inside of a tab (see {@link BookmarkPage}).
 */
public class BookmarkActivity extends BookmarkActivityBase implements SnackbarManageable {

    private BookmarkManager mBookmarkManager;
    private SnackbarManager mSnackbarManager;
    static final int EDIT_BOOKMARK_REQUEST_CODE = 14;
    public static final String INTENT_VISIT_BOOKMARK_ID = "BookmarkEditActivity.VisitBookmarkId";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
        mSnackbarManager = new SnackbarManager(getWindow());
        mBookmarkManager = new BookmarkManager(this, true);
        String url = getIntent().getDataString();
        if (TextUtils.isEmpty(url)) url = UrlConstants.BOOKMARKS_URL;
        mBookmarkManager.updateForUrl(url);

        setContentView(mBookmarkManager.getView());
        BookmarkUtils.setTaskDescriptionInDocumentMode(
                this, getString(OfflinePageUtils.getStringId(R.string.bookmarks)));

        // Hack to work around inferred theme false lint error: http://crbug.com/445633
        assert (R.layout.eb_main_content != 0);
    }

    @Override
    protected void onStart() {
        super.onStart();
        mSnackbarManager.onStart();
    }

    @Override
    protected void onStop() {
        super.onStop();
        mSnackbarManager.onStop();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        mBookmarkManager.destroy();
    }

    @Override
    public SnackbarManager getSnackbarManager() {
        return mSnackbarManager;
    }

    @Override
    public void onBackPressed() {
        if (!mBookmarkManager.onBackPressed()) super.onBackPressed();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == EDIT_BOOKMARK_REQUEST_CODE && resultCode == RESULT_OK) {
            BookmarkId bookmarkId = BookmarkId.getBookmarkIdFromString(data.getStringExtra(
                    INTENT_VISIT_BOOKMARK_ID));
            mBookmarkManager.openBookmark(bookmarkId, BookmarkLaunchLocation.BOOKMARK_EDITOR);
        }
    }
}

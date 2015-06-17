// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enhancedbookmarks;

import android.os.Bundle;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarManageable;

/**
 * The activity that wraps all enhanced bookmark UI on the phone. It keeps a
 * {@link EnhancedBookmarkManager} inside of it and creates a snackbar manager. This activity
 * should only be shown on phones; on tablet the enhanced bookmark UI is shown inside of a tab (see
 * {@link EnhancedBookmarkPage}).
 */
public class EnhancedBookmarkActivity extends EnhancedBookmarkActivityBase implements
        SnackbarManageable {

    private EnhancedBookmarkManager mBookmarkManager;
    private SnackbarManager mSnackbarManager;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mSnackbarManager = new SnackbarManager(findViewById(android.R.id.content));
        mBookmarkManager = new EnhancedBookmarkManager(this);
        setContentView(mBookmarkManager.getView());
        getWindow().setBackgroundDrawable(null);
        EnhancedBookmarkUtils.setTaskDescriptionInDocumentMode(this, getString(R.string.bookmarks));

        // Hack to work around inferred theme false lint error: http://crbug.com/445633
        assert (R.layout.eb_main_content != 0);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        mBookmarkManager.destroy();
        mSnackbarManager.dismissSnackbar(false);
    }

    @Override
    public SnackbarManager getSnackbarManager() {
        return mSnackbarManager;
    }

    @Override
    public void onBackPressed() {
        if (!mBookmarkManager.onBackPressed()) super.onBackPressed();
    }
}

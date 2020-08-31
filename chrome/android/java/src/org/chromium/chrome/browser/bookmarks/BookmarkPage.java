// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.embedder_support.util.UrlConstants;

/**
 * A native page holding a {@link BookmarkManager} on _tablet_.
 */
public class BookmarkPage extends BasicNativePage {
    private BookmarkManager mManager;
    private String mTitle;

    /**
     * Create a new instance of the bookmarks page.
     * @param activity The activity to get context and manage fragments.
     * @param host A NativePageHost to load urls.
     */
    public BookmarkPage(ChromeActivity activity, NativePageHost host) {
        super(host);

        mManager = new BookmarkManager(activity, false, activity.getSnackbarManager());
        mManager.setBasicNativePage(this);
        mTitle = host.getContext().getResources().getString(R.string.bookmarks);

        initWithView(mManager.getView());
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public String getHost() {
        return UrlConstants.BOOKMARKS_HOST;
    }

    @Override
    public void updateForUrl(String url) {
        super.updateForUrl(url);
        mManager.updateForUrl(url);
    }

    @Override
    public void destroy() {
        mManager.onDestroyed();
        mManager = null;
        super.destroy();
    }

    @VisibleForTesting
    public BookmarkManager getManagerForTesting() {
        return mManager;
    }
}

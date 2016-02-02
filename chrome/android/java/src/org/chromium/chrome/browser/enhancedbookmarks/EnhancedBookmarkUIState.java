// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enhancedbookmarks;

import android.net.Uri;
import android.text.TextUtils;

import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.components.bookmarks.BookmarkId;

/**
 * A class representing the UI state of the {@link EnhancedBookmarkManager}. All
 * states can be uniquely identified by a URL.
 */
class EnhancedBookmarkUIState {
    static final String URI_PERSIST_QUERY_NAME = "persist";

    static final int STATE_LOADING = 1;
    static final int STATE_ALL_BOOKMARKS = 2;
    static final int STATE_FOLDER = 3;
    static final int STATE_FILTER = 4;
    private static final int STATE_INVALID = 0;

    /**
     * One of the STATE_* constants.
     */
    int mState;
    String mUrl;
    /** Whether this state should be persisted as user's last location. */
    boolean mShouldPersist = true;
    BookmarkId mFolder;
    EnhancedBookmarkFilter mFilter;

    static EnhancedBookmarkUIState createLoadingState() {
        EnhancedBookmarkUIState state = new EnhancedBookmarkUIState();
        state.mState = STATE_LOADING;
        state.mShouldPersist = false;
        state.mUrl = "";
        return state;
    }

    static EnhancedBookmarkUIState createAllBookmarksState(EnhancedBookmarksModel bookmarkModel) {
        return createStateFromUrl(Uri.parse(UrlConstants.BOOKMARKS_URL), bookmarkModel);
    }

    static EnhancedBookmarkUIState createFolderState(BookmarkId folder,
            EnhancedBookmarksModel bookmarkModel) {
        return createStateFromUrl(createFolderUrl(folder), bookmarkModel);
    }

    static EnhancedBookmarkUIState createFilterState(
            EnhancedBookmarkFilter filter, EnhancedBookmarksModel bookmarkModel) {
        return createStateFromUrl(createFilterUrl(filter, true), bookmarkModel);
    }

    /**
     * @see #createStateFromUrl(Uri, EnhancedBookmarksModel)
     */
    static EnhancedBookmarkUIState createStateFromUrl(String url,
            EnhancedBookmarksModel bookmarkModel) {
        return createStateFromUrl(Uri.parse(url), bookmarkModel);
    }

    /**
     * @return A state corresponding to the URI object. If the URI is not valid,
     *         return all_bookmarks.
     */
    static EnhancedBookmarkUIState createStateFromUrl(Uri uri,
            EnhancedBookmarksModel bookmarkModel) {
        EnhancedBookmarkUIState state = new EnhancedBookmarkUIState();
        state.mState = STATE_INVALID;
        state.mUrl = uri.toString();
        state.mShouldPersist = shouldPersist(uri);

        if (state.mUrl.equals(UrlConstants.BOOKMARKS_URL)) {
            state.mState = STATE_ALL_BOOKMARKS;
        } else if (state.mUrl.startsWith(UrlConstants.BOOKMARKS_FOLDER_URL)) {
            String path = uri.getLastPathSegment();
            if (!path.isEmpty()) {
                state.mFolder = BookmarkId.getBookmarkIdFromString(path);
                state.mState = STATE_FOLDER;
            }
        } else if (state.mUrl.startsWith(UrlConstants.BOOKMARKS_FILTER_URL)) {
            String path = uri.getLastPathSegment();
            if (!path.isEmpty()) {
                state.mState = STATE_FILTER;
                state.mFilter = EnhancedBookmarkFilter.valueOf(path);
            }
        }

        if (!state.isValid(bookmarkModel)) {
            state.mState = STATE_ALL_BOOKMARKS;
            state.mUrl = UrlConstants.BOOKMARKS_URL;
        }

        return state;
    }

    static Uri createFolderUrl(BookmarkId folderId) {
        return createUrl(UrlConstants.BOOKMARKS_FOLDER_URL, folderId.toString(), true);
    }

    static Uri createFilterUrl(EnhancedBookmarkFilter filter, boolean shouldPersist) {
        return createUrl(UrlConstants.BOOKMARKS_FILTER_URL, filter.value, shouldPersist);
    }

    /**
     * Encodes the path and appends it to the base url. A simple appending
     * does not work because there might be spaces in suffix.
     * @param shouldPersist Whether this url should be saved to preferences as
     *                      user's last location.
     */
    private static Uri createUrl(String baseUrl, String pathSuffix, boolean shouldPersist) {
        Uri.Builder builder = Uri.parse(baseUrl).buildUpon();
        builder.appendPath(pathSuffix);
        if (!shouldPersist) {
            builder.appendQueryParameter(URI_PERSIST_QUERY_NAME, "0");
        }
        return builder.build();
    }

    private static boolean shouldPersist(Uri uri) {
        String queryString = uri.getQueryParameter(URI_PERSIST_QUERY_NAME);
        return !("0".equals(queryString));
    }

    private EnhancedBookmarkUIState() {}

    @Override
    public int hashCode() {
        return 31 * mUrl.hashCode() + mState;
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof EnhancedBookmarkUIState)) return false;
        EnhancedBookmarkUIState other = (EnhancedBookmarkUIState) obj;
        return mState == other.mState && TextUtils.equals(mUrl, other.mUrl);
    }

    /**
     * @return Whether this state is valid.
     */
    boolean isValid(EnhancedBookmarksModel bookmarkModel) {
        if (mUrl == null || mState == STATE_INVALID) return false;

        if (mState == STATE_FOLDER) {
            return mFolder != null && bookmarkModel.doesBookmarkExist(mFolder)
                    && !mFolder.equals(bookmarkModel.getRootFolderId());
        }

        if (mState == STATE_FILTER) {
            if (mFilter == null) return false;
            if (mFilter == EnhancedBookmarkFilter.OFFLINE_PAGES) {
                return OfflinePageBridge.isEnabled();
            }
        }

        return true;
    }
}

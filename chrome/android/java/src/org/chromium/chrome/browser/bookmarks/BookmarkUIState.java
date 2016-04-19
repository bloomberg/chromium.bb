// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.net.Uri;
import android.text.TextUtils;

import org.chromium.chrome.browser.UrlConstants;
import org.chromium.components.bookmarks.BookmarkId;

/**
 * A class representing the UI state of the {@link BookmarkManager}. All
 * states can be uniquely identified by a URL.
 */
class BookmarkUIState {
    static final String URI_PERSIST_QUERY_NAME = "persist";

    static final int STATE_LOADING = 1;
    static final int STATE_ALL_BOOKMARKS = 2;
    static final int STATE_FOLDER = 3;
    private static final int STATE_INVALID = 0;

    /**
     * One of the STATE_* constants.
     */
    int mState;
    String mUrl;
    BookmarkId mFolder;

    static BookmarkUIState createLoadingState() {
        BookmarkUIState state = new BookmarkUIState();
        state.mState = STATE_LOADING;
        state.mUrl = "";
        return state;
    }

    static BookmarkUIState createAllBookmarksState(BookmarkModel bookmarkModel) {
        return createStateFromUrl(Uri.parse(UrlConstants.BOOKMARKS_URL), bookmarkModel);
    }

    static BookmarkUIState createFolderState(BookmarkId folder,
            BookmarkModel bookmarkModel) {
        return createStateFromUrl(createFolderUrl(folder), bookmarkModel);
    }

    /**
     * @see #createStateFromUrl(Uri, BookmarkModel)
     */
    static BookmarkUIState createStateFromUrl(String url, BookmarkModel bookmarkModel) {
        return createStateFromUrl(Uri.parse(url), bookmarkModel);
    }

    /**
     * @return A state corresponding to the URI object. If the URI is not valid,
     *         return all_bookmarks.
     */
    static BookmarkUIState createStateFromUrl(Uri uri, BookmarkModel bookmarkModel) {
        BookmarkUIState state = new BookmarkUIState();
        state.mState = STATE_INVALID;
        state.mUrl = uri.toString();

        if (state.mUrl.equals(UrlConstants.BOOKMARKS_URL)) {
            state.mState = STATE_ALL_BOOKMARKS;
        } else if (state.mUrl.startsWith(UrlConstants.BOOKMARKS_FOLDER_URL)) {
            String path = uri.getLastPathSegment();
            if (!path.isEmpty()) {
                state.mFolder = BookmarkId.getBookmarkIdFromString(path);
                state.mState = STATE_FOLDER;
            }
        }

        if (!state.isValid(bookmarkModel)) {
            state.mState = STATE_ALL_BOOKMARKS;
            state.mUrl = UrlConstants.BOOKMARKS_URL;
        }

        return state;
    }

    static Uri createFolderUrl(BookmarkId folderId) {
        Uri.Builder builder = Uri.parse(UrlConstants.BOOKMARKS_FOLDER_URL).buildUpon();
        // Encodes the path and appends it to the base url. A simple appending
        // does not work because there might be spaces in suffix.
        builder.appendPath(folderId.toString());
        return builder.build();
    }

    private BookmarkUIState() {}

    @Override
    public int hashCode() {
        return 31 * mUrl.hashCode() + mState;
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof BookmarkUIState)) return false;
        BookmarkUIState other = (BookmarkUIState) obj;
        return mState == other.mState && TextUtils.equals(mUrl, other.mUrl);
    }

    /**
     * @return Whether this state is valid.
     */
    boolean isValid(BookmarkModel bookmarkModel) {
        if (mUrl == null || mState == STATE_INVALID) return false;

        if (mState == STATE_FOLDER) {
            return mFolder != null && bookmarkModel.doesBookmarkExist(mFolder)
                    && !mFolder.equals(bookmarkModel.getRootFolderId());
        }

        return true;
    }
}

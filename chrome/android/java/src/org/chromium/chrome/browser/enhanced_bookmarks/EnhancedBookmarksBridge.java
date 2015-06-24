// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enhanced_bookmarks;

import org.chromium.base.JNINamespace;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;

/**
 * Access gate to C++ side enhanced bookmarks functionalities.
 */
@JNINamespace("enhanced_bookmarks::android")
public final class EnhancedBookmarksBridge {

    private long mNativeEnhancedBookmarksBridge;

    /**
     * Creates a new enhanced bridge using the given profile.
     */
    public EnhancedBookmarksBridge(Profile profile) {
        mNativeEnhancedBookmarksBridge = nativeInit(profile);
    }

    public void destroy() {
        assert mNativeEnhancedBookmarksBridge != 0;
        nativeDestroy(mNativeEnhancedBookmarksBridge);
        mNativeEnhancedBookmarksBridge = 0;
    }

    /**
     * Adds a folder to the EnhancedBookmarkModel
     * @param parent The parent of this folder
     * @param index The position this folder should appear within the parent
     * @param title The title of the bookmark
     * @return The ID of the newly created folder.
     */
    public BookmarkId addFolder(BookmarkId parent, int index, String title) {
        return nativeAddFolder(mNativeEnhancedBookmarksBridge, parent, index, title);
    }

    /**
     * Adds a Bookmark to the EnhancedBookmarkModel
     * @param parent The parent of this bookmark
     * @param index The position this bookmark should appear within the parent
     * @param title The title of the bookmark
     * @param url URL of the bookmark
     * @return The ID of the newly created bookmark
     */
    public BookmarkId addBookmark(BookmarkId parent, int index, String title, String url) {
        return nativeAddBookmark(mNativeEnhancedBookmarksBridge, parent, index, title, url);
    }

    /**
     * Moves a bookmark to another folder, and append it at the end of the list of all children.
     * @param bookmarkId The item to be be moved
     * @param newParentId The new parent of the item
     */
    public void moveBookmark(BookmarkId bookmarkId, BookmarkId newParentId) {
        nativeMoveBookmark(mNativeEnhancedBookmarksBridge, bookmarkId, newParentId);
    }

    private native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeEnhancedBookmarksBridge);
    private native BookmarkId nativeAddFolder(long nativeEnhancedBookmarksBridge, BookmarkId parent,
            int index, String title);
    private native void nativeMoveBookmark(long nativeEnhancedBookmarksBridge,
            BookmarkId bookmarkId, BookmarkId newParentId);
    private native BookmarkId nativeAddBookmark(long nativeEnhancedBookmarksBridge,
            BookmarkId parent, int index, String title, String url);
}

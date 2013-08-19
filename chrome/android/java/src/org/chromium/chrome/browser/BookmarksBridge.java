// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.base.CalledByNative;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;

/**
 * Handler to fetch the bookmarks, titles, urls and folder hierarchy.
 */
public class BookmarksBridge {

    private final Profile mProfile;
    private int mNativeBookmarksBridge;
    private boolean mIsNativeBookmarkModelLoaded;
    private List<DelayedBookmarkCallback> mDelayedBookmarkCallbacks =
            new ArrayList<DelayedBookmarkCallback>();

    /**
     * Interface for callback object for fetching bookmarks and folder hierarchy.
     */
    public interface BookmarksCallback {

        /**
         * Callback method for fetching bookmarks for a folder and the folder hierarchy.
         * @param folderId The folder id to which the bookmarks belong.
         * @param bookmarksList List holding the fetched bookmarks and details.
         */
        @CalledByNative("BookmarksCallback")
        public void onBookmarksAvailable(long folderId, List<BookmarkItem> bookmarksList);

        /**
         * Callback method for fetching the folder hierarchy.
         * @param folderId The folder id to which the bookmarks belong.
         * @param bookmarksList List holding the fetched folder details.
         */
        @CalledByNative("BookmarksCallback")
        public void onBookmarksFolderHierarchyAvailable(long folderId,
                List<BookmarkItem> bookmarksList);

   }

    /**
     * Handler to fetch the bookmarks, titles, urls and folder hierarchy.
     * @param profile Profile instance corresponding to the active profile.
     */
    public BookmarksBridge(Profile profile) {
        mProfile = profile;
        mNativeBookmarksBridge = nativeInit(profile);
    }

    /**
     * Destroys this instance so no further calls can be executed.
     */
    public void destroy() {
        if (mNativeBookmarksBridge != 0) {
            nativeDestroy(mNativeBookmarksBridge);
            mNativeBookmarksBridge = 0;
            mIsNativeBookmarkModelLoaded = false;
        }
    }

    /**
     * Fetches the bookmarks of the current folder. Callback will be
     * synchronous if the bookmark model is already loaded and async if it is loaded in the
     * background.
     * @param folderId The current folder id.
     * @param callback Instance of a callback object.
     */
    public void getBookmarksForFolder(long folderId, BookmarksCallback callback) {
        if (mIsNativeBookmarkModelLoaded) {
            nativeGetBookmarksForFolder(mNativeBookmarksBridge, folderId, callback,
                    new ArrayList<BookmarkItem>());
        } else {
            mDelayedBookmarkCallbacks.add(new DelayedBookmarkCallback(folderId, callback,
                    DelayedBookmarkCallback.GET_BOOKMARKS_FOR_FOLDER, this));
        }
    }

    /**
     * Fetches the folder hierarchy of the given folder. Callback will be
     * synchronous if the bookmark model is already loaded and async if it is loaded in the
     * background.
     * @param folderId The current folder id.
     * @param callback Instance of a callback object.
     */
    public void getCurrentFolderHierarchy(long folderId, BookmarksCallback callback) {
        if (mIsNativeBookmarkModelLoaded) {
            nativeGetCurrentFolderHierarchy(mNativeBookmarksBridge, folderId, callback,
                    new ArrayList<BookmarkItem>());
        } else {
            mDelayedBookmarkCallbacks.add(new DelayedBookmarkCallback(folderId, callback,
                    DelayedBookmarkCallback.GET_CURRENT_FOLDER_HIERARCHY, this));
        }
    }

    @CalledByNative
    public void bookmarkModelLoaded() {
        mIsNativeBookmarkModelLoaded = true;
        if (!mDelayedBookmarkCallbacks.isEmpty()) {
            for (int i = 0; i < mDelayedBookmarkCallbacks.size(); i++) {
                mDelayedBookmarkCallbacks.get(i).callCallbackMethod();
            }
            mDelayedBookmarkCallbacks.clear();
        }
    }

    @CalledByNative
    public void bookmarkModelDeleted() {
        destroy();
    }

    @CalledByNative
    private static void create(List<BookmarkItem> bookmarksList, long id, String title, String url,
            boolean isFolder, long parentId) {
        bookmarksList.add(new BookmarkItem(id, title, url, isFolder, parentId));
    }

    private native void nativeGetBookmarksForFolder(int nativeBookmarksBridge,
            long folderId, BookmarksCallback callback,
            List<BookmarkItem> bookmarksList);
    private native void nativeGetCurrentFolderHierarchy(int nativeBookmarksBridge,
            long folderId, BookmarksCallback callback,
            List<BookmarkItem> bookmarksList);
    private native int nativeInit(Profile profile);
    private native void nativeDestroy(int nativeBookmarksBridge);

    /**
     * Simple object representing the bookmark item.
     */
    public static class BookmarkItem {

        private final String mTitle;
        private final String mUrl;
        private final long mId;
        private final boolean mIsFolder;
        private final long mParentId;

        private BookmarkItem(long id, String title, String url, boolean isFolder, long parentId) {
            mId = id;
            mTitle = title;
            mUrl = url;
            mIsFolder = isFolder;
            mParentId = parentId;
        }

        /**
         * @return Title of the bookmark item.
         */
        public String getTitle() {
            return mTitle;
        }

        /**
         * @return Url of the bookmark item.
         */
        public String getUrl() {
            return mUrl;
        }

        /**
         * @return Id of the bookmark item.
         */
        public long getId() {
            return mId;
        }

        /**
         * @return Whether item is a folder or a bookmark.
         */
        public boolean isFolder() {
            return mIsFolder;
        }

        /**
         * @return Parent id of the bookmark item.
         */
        public long getParentId() {
            return mParentId;
        }
    }

    /**
     * Details about callbacks that need to be called once the bookmark model has loaded.
     */
    private static class DelayedBookmarkCallback {

        private static final int GET_BOOKMARKS_FOR_FOLDER = 0;
        private static final int GET_CURRENT_FOLDER_HIERARCHY = 1;

        private final BookmarksCallback mCallback;
        private final long mFolderId;
        private final int mCallbackMethod;
        private final BookmarksBridge mHandler;

        private DelayedBookmarkCallback(long folderId, BookmarksCallback callback, int method,
                BookmarksBridge handler) {
            mFolderId = folderId;
            mCallback = callback;
            mCallbackMethod = method;
            mHandler = handler;
        }

        /**
         * Invoke the callback method.
         */
        private void callCallbackMethod() {
            switch(mCallbackMethod) {
                case GET_BOOKMARKS_FOR_FOLDER:
                    mHandler.getBookmarksForFolder(mFolderId, mCallback);
                    break;
                case GET_CURRENT_FOLDER_HIERARCHY:
                    mHandler.getCurrentFolderHierarchy(mFolderId, mCallback);
                    break;
                default:
                    break;
            }
        }
    }

}


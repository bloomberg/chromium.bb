// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.base.CalledByNative;
import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;

/**
 * Provides the communication channel for Android to fetch and manipulate the
 * bookmark model stored in native.
 */
public class BookmarksBridge {

    private final Profile mProfile;
    private int mNativeBookmarksBridge;
    private boolean mIsNativeBookmarkModelLoaded;
    private List<DelayedBookmarkCallback> mDelayedBookmarkCallbacks
            = new ArrayList<DelayedBookmarkCallback>();
    private ObserverList<BookmarkModelObserver> mObservers
            = new ObserverList<BookmarkModelObserver>();

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
        void onBookmarksAvailable(long folderId, List<BookmarkItem> bookmarksList);

        /**
         * Callback method for fetching the folder hierarchy.
         * @param folderId The folder id to which the bookmarks belong.
         * @param bookmarksList List holding the fetched folder details.
         */
        @CalledByNative("BookmarksCallback")
        void onBookmarksFolderHierarchyAvailable(long folderId,
                List<BookmarkItem> bookmarksList);
    }

    /**
     * Interface that provides listeners to be notified of changes to the bookmark model.
     */
    public interface BookmarkModelObserver {
        /**
         * Invoked when a node has moved.
         * @param oldParent The parent before the move.
         * @param oldIndex The index of the node in the old parent.
         * @param newParent The parent after the move.
         * @param newIndex The index of the node in the new parent.
         */
        void bookmarkNodeMoved(
                BookmarkItem oldParent, int oldIndex, BookmarkItem newParent, int newIndex);

        /**
         * Invoked when a node has been added.
         * @param parent The parent of the node being added.
         * @param index The index of the added node.
         */
        void bookmarkNodeAdded(BookmarkItem parent, int index);

        /**
         * Invoked when a node has been removed, the item may still be starred though.
         * @param parent The parent of the node that was removed.
         * @param oldIndex The index of the removed node in the parent before it was removed.
         * @param node The node that was removed.
         */
        void bookmarkNodeRemoved(BookmarkItem parent, int oldIndex, BookmarkItem node);

        /**
         * Invoked when the title or url of a node changes.
         * @param node The node being changed.
         */
        void bookmarkNodeChanged(BookmarkItem node);

        /**
         * Invoked when the children (just direct children, not descendants) of a node have been
         * reordered in some way, such as sorted.
         * @param node The node whose children are being reordered.
         */
        void bookmarkNodeChildrenReordered(BookmarkItem node);

        /**
         * Invoked before an extensive set of model changes is about to begin.  This tells UI
         * intensive observers to wait until the updates finish to update themselves. These methods
         * should only be used for imports and sync. Observers should still respond to
         * BookmarkNodeRemoved immediately, to avoid holding onto stale node references.
         */
        void extensiveBookmarkChangesBeginning();

        /**
         * Invoked after an extensive set of model changes has ended.  This tells observers to
         * update themselves if they were waiting for the update to finish.
         */
        void extensiveBookmarkChangesEnded();
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
        mObservers.clear();
    }

    /**
     * Add an observer to bookmark model changes.
     * @param observer The observer to be added.
     */
    public void addObserver(BookmarkModelObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Remove an observer of bookmark model changes.
     * @param observer The observer to be removed.
     */
    public void removeObserver(BookmarkModelObserver observer) {
        mObservers.removeObserver(observer);
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

    /**
     * Deletes a specified bookmark node.
     * @param bookmarkId The ID of the bookmark to be deleted.
     */
    public void deleteBookmark(long bookmarkId) {
        nativeDeleteBookmark(mNativeBookmarksBridge, bookmarkId);
    }

    @CalledByNative
    private void bookmarkModelLoaded() {
        mIsNativeBookmarkModelLoaded = true;
        if (!mDelayedBookmarkCallbacks.isEmpty()) {
            for (int i = 0; i < mDelayedBookmarkCallbacks.size(); i++) {
                mDelayedBookmarkCallbacks.get(i).callCallbackMethod();
            }
            mDelayedBookmarkCallbacks.clear();
        }
    }

    @CalledByNative
    private void bookmarkModelDeleted() {
        destroy();
    }

    @CalledByNative
    private void bookmarkNodeMoved(
            BookmarkItem oldParent, int oldIndex, BookmarkItem newParent, int newIndex) {
        for (BookmarkModelObserver observer : mObservers) {
            observer.bookmarkNodeMoved(oldParent, oldIndex, newParent, newIndex);
        }
    }

    @CalledByNative
    private void bookmarkNodeAdded(BookmarkItem parent, int index) {
        for (BookmarkModelObserver observer : mObservers) {
            observer.bookmarkNodeAdded(parent, index);
        }
    }

    @CalledByNative
    private void bookmarkNodeRemoved(BookmarkItem parent, int oldIndex, BookmarkItem node) {
        for (BookmarkModelObserver observer : mObservers) {
            observer.bookmarkNodeRemoved(parent, oldIndex, node);
        }
    }

    @CalledByNative
    private void bookmarkNodeChanged(BookmarkItem node) {
        for (BookmarkModelObserver observer : mObservers) {
            observer.bookmarkNodeChanged(node);
        }
    }

    @CalledByNative
    private void bookmarkNodeChildrenReordered(BookmarkItem node) {
        for (BookmarkModelObserver observer : mObservers) {
            observer.bookmarkNodeChildrenReordered(node);
        }
    }

    @CalledByNative
    private void extensiveBookmarkChangesBeginning() {
        for (BookmarkModelObserver observer : mObservers) {
            observer.extensiveBookmarkChangesBeginning();
        }
    }

    @CalledByNative
    private void extensiveBookmarkChangesEnded() {
        for (BookmarkModelObserver observer : mObservers) {
            observer.extensiveBookmarkChangesEnded();
        }
    }

    @CalledByNative
    private static BookmarkItem create(long id, String title, String url,
            boolean isFolder, long parentId, boolean isEditable) {
        return new BookmarkItem(id, title, url, isFolder, parentId, isEditable);
    }

    @CalledByNative
    private static void addToList(List<BookmarkItem> bookmarksList, BookmarkItem bookmark) {
        bookmarksList.add(bookmark);
    }

    private native void nativeGetBookmarksForFolder(int nativeBookmarksBridge,
            long folderId, BookmarksCallback callback,
            List<BookmarkItem> bookmarksList);
    private native void nativeGetCurrentFolderHierarchy(int nativeBookmarksBridge,
            long folderId, BookmarksCallback callback,
            List<BookmarkItem> bookmarksList);
    private native void nativeDeleteBookmark(int nativeBookmarksBridge, long bookmarkId);
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
        private final boolean mIsEditable;

        private BookmarkItem(long id, String title, String url, boolean isFolder, long parentId,
                boolean isEditable) {
            mId = id;
            mTitle = title;
            mUrl = url;
            mIsFolder = isFolder;
            mParentId = parentId;
            mIsEditable = isEditable;
        }

        /** @return Title of the bookmark item. */
        public String getTitle() {
            return mTitle;
        }

        /** @return Url of the bookmark item. */
        public String getUrl() {
            return mUrl;
        }

        /** @return Id of the bookmark item. */
        public long getId() {
            return mId;
        }

        /** @return Whether item is a folder or a bookmark. */
        public boolean isFolder() {
            return mIsFolder;
        }

        /** @return Parent id of the bookmark item. */
        public long getParentId() {
            return mParentId;
        }

        /** @return Whether this bookmark can be edited. */
        public boolean isEditable() {
            return mIsEditable;
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


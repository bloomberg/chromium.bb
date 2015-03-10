// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enhanced_bookmarks;

import org.chromium.base.ObserverList;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.BookmarksBridge;
import org.chromium.chrome.browser.BookmarksBridge.BookmarkItem;
import org.chromium.chrome.browser.BookmarksBridge.BookmarkModelObserver;
import org.chromium.chrome.browser.enhanced_bookmarks.EnhancedBookmarksBridge.FiltersObserver;
import org.chromium.chrome.browser.enhanced_bookmarks.EnhancedBookmarksBridge.SalientImageCallback;
import org.chromium.chrome.browser.enhanced_bookmarks.EnhancedBookmarksBridge.SearchServiceObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkMatch;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.content_public.browser.WebContents;

import java.util.List;

/**
 * A class that encapsulates all the (enhanced) bookmark model related logic. Essentially, this is a
 * collection of all the bookmark bridges with helper functions. It is recommended to create a new
 * instance of this class, instead of passing it around. However, be sure to call
 * {@link EnhancedBookmarksModel#destroy()} once you are done with it.
 */
public class EnhancedBookmarksModel {

    /**
     * Observer that listens to delete event. This interface is used by undo controllers to know
     * which bookmarks were deleted. Note this observer only listens to events that go through
     * enhanced bookmark model.
     */
    public interface EnhancedBookmarkDeleteObserver {

        /**
         * Callback being triggered immediately before bookmarks are deleted.
         * @param titles All titles of the bookmarks to be deleted.
         * @param isUndoable Whether the deletion is undoable.
         */
        void onDeleteBookmarks(String[] titles, boolean isUndoable);
    }

    private final BookmarksBridge mBookmarksBridge;
    private final EnhancedBookmarksBridge mEnhancedBookmarksBridge;
    private ObserverList<EnhancedBookmarkDeleteObserver> mDeleteObservers = new ObserverList<>();

    /**
     * Initialize enhanced bookmark model for last used non-incognito profile with a java-side image
     * cache that has the given size.
     */
    public EnhancedBookmarksModel() {
        Profile originalProfile = Profile.getLastUsedProfile().getOriginalProfile();
        mBookmarksBridge = new BookmarksBridge(originalProfile);
        mEnhancedBookmarksBridge = new EnhancedBookmarksBridge(originalProfile);
    }

    @VisibleForTesting
    public EnhancedBookmarksModel(Profile profile) {
        mBookmarksBridge = new BookmarksBridge(profile);
        mEnhancedBookmarksBridge = new EnhancedBookmarksBridge(profile);
    }

    /**
     * Clean up all the bridges. This must be called after done using this class.
     */
    public void destroy() {
        mBookmarksBridge.destroy();
        mEnhancedBookmarksBridge.destroy();
    }

    /**
     * @see BookmarksBridge#addObserver(BookmarkModelObserver)
     */
    public void addModelObserver(BookmarkModelObserver observer) {
        mBookmarksBridge.addObserver(observer);
    }

    /**
     * @see BookmarksBridge#removeObserver(BookmarkModelObserver)
     */
    public void removeModelObserver(BookmarkModelObserver observer) {
        mBookmarksBridge.removeObserver(observer);
    }

    /**
     * @see BookmarksBridge#isBookmarkModelLoaded()
     */
    public boolean isBookmarkModelLoaded() {
        return mBookmarksBridge.isBookmarkModelLoaded();
    }

    /**
     * @see BookmarksBridge#getBookmarkById(BookmarkId)
     */
    public BookmarkItem getBookmarkById(BookmarkId id) {
        return mBookmarksBridge.getBookmarkById(id);
    }

    /**
     * Add an observer that listens to delete events that go through enhanced bookmark model.
     * @param observer The observer to add.
     */
    public void addDeleteObserver(EnhancedBookmarkDeleteObserver observer) {
        mDeleteObservers.addObserver(observer);
    }

    /**
     * Remove the observer from listening to bookmark deleting events.
     * @param observer The observer to remove.
     */
    public void removeDeleteObserver(EnhancedBookmarkDeleteObserver observer) {
        mDeleteObservers.removeObserver(observer);
    }

    /**
     * Delete one or multiple bookmarks from model. If more than one bookmarks are passed here, this
     * method will group these delete operations into one undo bundle so that later if the user
     * clicks undo, all bookmarks deleted here will be restored.
     * @param bookmarks Bookmarks to delete. Note this array should not contain a folder and its
     *                  children, because deleting folder will also remove all its children, and
     *                  deleting children once more will cause errors.
     */
    public void deleteBookmarks(BookmarkId... bookmarks) {
        assert bookmarks != null && bookmarks.length > 0;
        // Store all titles of bookmarks.
        String[] titles = new String[bookmarks.length];
        boolean isUndoable = true;
        for (int i = 0; i < bookmarks.length; i++) {
            titles[i] = getBookmarkTitle(bookmarks[i]);
            isUndoable &= (bookmarks[i].getType() == BookmarkType.NORMAL);
        }

        if (bookmarks.length == 1) {
            mBookmarksBridge.deleteBookmark(bookmarks[0]);
        } else {
            mBookmarksBridge.startGroupingUndos();
            for (BookmarkId bookmark : bookmarks) {
                mBookmarksBridge.deleteBookmark(bookmark);
            }
            mBookmarksBridge.endGroupingUndos();
        }

        for (EnhancedBookmarkDeleteObserver observer : mDeleteObservers) {
            observer.onDeleteBookmarks(titles, isUndoable);
        }
    }

    /**
     * @see BookmarksBridge#getTopLevelFolderParentIDs()
     */
    public List<BookmarkId> getTopLevelFolderParentIDs() {
        return mBookmarksBridge.getTopLevelFolderParentIDs();
    }

    /**
     * @see BookmarksBridge#getTopLevelFolderIDs(boolean, boolean)
     */
    public List<BookmarkId> getTopLevelFolderIDs(boolean getSpecial, boolean getNormal) {
        return mBookmarksBridge.getTopLevelFolderIDs(getSpecial, getNormal);
    }

    /**
     * @see BookmarksBridge#getAllFoldersWithDepths(List, List)
     */
    public void getAllFoldersWithDepths(List<BookmarkId> bookmarkList, List<Integer> depthList) {
        mBookmarksBridge.getAllFoldersWithDepths(bookmarkList, depthList);
    }

    /**
     * @see BookmarksBridge#getMoveDestinations(List, List, List)
     */
    public void getMoveDestinations(List<BookmarkId> bookmarkList, List<Integer> depthList,
            List<BookmarkId> bookmarksToMove) {
        mBookmarksBridge.getMoveDestinations(bookmarkList, depthList, bookmarksToMove);
    }

    /**
     * @see BookmarksBridge#getRootFolderId()
     */
    public BookmarkId getRootFolderId() {
        return mBookmarksBridge.getRootFolderId();
    }

    /**
     * @see BookmarksBridge#getOtherFolderId()
     */
    public BookmarkId getOtherFolderId() {
        return mBookmarksBridge.getOtherFolderId();
    }

    /**
     * @see BookmarksBridge#getMobileFolderId()
     */
    public BookmarkId getMobileFolderId() {
        return mBookmarksBridge.getMobileFolderId();
    }

    /**
     * @see BookmarksBridge#getDesktopFolderId()
     */
    public BookmarkId getDesktopFolderId() {
        return mBookmarksBridge.getDesktopFolderId();
    }

    /**
     * @return The id of the default folder to add bookmarks/folders to.
     */
    public BookmarkId getDefaultFolder() {
        return mBookmarksBridge.getMobileFolderId();
    }

    /**
     * @see BookmarksBridge#getChildIDs(BookmarkId, boolean, boolean)
     */
    public List<BookmarkId> getChildIDs(BookmarkId id, boolean getFolders, boolean getBookmarks) {
        return mBookmarksBridge.getChildIDs(id, getFolders, getBookmarks);
    }

    public BookmarkId getChildAt(BookmarkId folderId, int index) {
        return mBookmarksBridge.getChildAt(folderId, index);
    }

    /**
     * @see BookmarksBridge#getAllBookmarkIDsOrderedByCreationDate()
     */
    public List<BookmarkId> getAllBookmarkIDsOrderedByCreationDate() {
        return mBookmarksBridge.getAllBookmarkIDsOrderedByCreationDate();
    }

    /**
     * @see BookmarksBridge#doesBookmarkExist(BookmarkId)
     */
    public boolean doesBookmarkExist(BookmarkId id) {
        return mBookmarksBridge.doesBookmarkExist(id);
    }

    /**
     * @see BookmarksBridge#setBookmarkTitle(BookmarkId, String)
     */
    public void setBookmarkTitle(BookmarkId bookmark, String title) {
        mBookmarksBridge.setBookmarkTitle(bookmark, title);
    }

    /**
     * @see BookmarksBridge#setBookmarkUrl(BookmarkId, String)
     */
    public void setBookmarkUrl(BookmarkId bookmark, String url) {
        mBookmarksBridge.setBookmarkUrl(bookmark, url);
    }

    /**
     *
     * @see EnhancedBookmarksBridge#addFolder(BookmarkId, int, String)
     */
    public BookmarkId addFolder(BookmarkId parent, int index, String title) {
        return mEnhancedBookmarksBridge.addFolder(parent, index, title);
    }

    /**
     * @see EnhancedBookmarksBridge#addBookmark(BookmarkId, int, String, String)
     */
    public BookmarkId addBookmark(BookmarkId parent, int index, String title, String url) {
        url = DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(url);
        return mEnhancedBookmarksBridge.addBookmark(parent, index, title, url);
    }

    /**
     * Calls {@link EnhancedBookmarksBridge#moveBookmark(BookmarkId, BookmarkId)} in a reversed
     * order of the list, in order to let the last item appear at the top.
     */
    public void moveBookmarks(List<BookmarkId> bookmarkIds, BookmarkId newParentId) {
        for (int i = bookmarkIds.size() - 1; i >= 0; i--) {
            mEnhancedBookmarksBridge.moveBookmark(bookmarkIds.get(i), newParentId);
        }
    }

    /**
     * @see BookmarkItem#getTitle()
     */
    public String getBookmarkTitle(BookmarkId bookmarkId) {
        return mBookmarksBridge.getBookmarkById(bookmarkId).getTitle();
    }

    /**
     * @see EnhancedBookmarksBridge#getBookmarkDescription(BookmarkId)
     */
    public String getBookmarkDescription(BookmarkId id) {
        return mEnhancedBookmarksBridge.getBookmarkDescription(id);
    }

    /**
     * @see EnhancedBookmarksBridge#setBookmarkDescription(BookmarkId, String)
     */
    public void setBookmarkDescription(BookmarkId id, String description) {
        mEnhancedBookmarksBridge.setBookmarkDescription(id, description);
    }

    /**
     * @see EnhancedBookmarksBridge#salientImageForUrl(String, SalientImageCallback)
     */
    public boolean salientImageForUrl(String url, SalientImageCallback callback) {
        return mEnhancedBookmarksBridge.salientImageForUrl(url, callback);
    }

    /**
     * @see EnhancedBookmarksBridge#fetchImageForTab(WebContents)
     */
    public void fetchImageForTab(WebContents webContents) {
        mEnhancedBookmarksBridge.fetchImageForTab(webContents);
    }

    /**
     * @see BookmarksBridge#undo()
     */
    public void undo() {
        mBookmarksBridge.undo();
    }

    /**
     * @see EnhancedBookmarksBridge#getFiltersForBookmark(BookmarkId)
     */
    public String[] getFiltersForBookmark(BookmarkId bookmark) {
        return mEnhancedBookmarksBridge.getFiltersForBookmark(bookmark);
    }

    /**
     * @see EnhancedBookmarksBridge#getFilters()
     */
    public List<String> getFilters() {
        return mEnhancedBookmarksBridge.getFilters();
    }

    /**
     * @see EnhancedBookmarksBridge#getBookmarksForFilter(String)
     */
    public List<BookmarkId> getBookmarksForFilter(String filter) {
        return mEnhancedBookmarksBridge.getBookmarksForFilter(filter);
    }

    /**
     * @see EnhancedBookmarksBridge#addFiltersObserver(FiltersObserver)
     */
    public void addFiltersObserver(FiltersObserver observer) {
        mEnhancedBookmarksBridge.addFiltersObserver(observer);
    }

    /**
     * @see EnhancedBookmarksBridge#removeFiltersObserver(FiltersObserver)
     */
    public void removeFiltersObserver(FiltersObserver observer) {
        mEnhancedBookmarksBridge.removeFiltersObserver(observer);
    }

    /**
     * @see EnhancedBookmarksBridge#sendSearchRequest(String)
     */
    public void sendSearchRequest(String query) {
        mEnhancedBookmarksBridge.sendSearchRequest(query);
    }

    /**
     * @see EnhancedBookmarksBridge#sendSearchRequest(String)
     */
    public List<BookmarkId> getSearchResultsForQuery(String query) {
        return mEnhancedBookmarksBridge.getSearchResultsForQuery(query);
    }

    /**
     * @see BookmarksBridge#searchBookmarks(String, int)
     */
    public List<BookmarkMatch> getLocalSearchForQuery(String query, int maxNumberOfQuery) {
        return mBookmarksBridge.searchBookmarks(query, maxNumberOfQuery);
    }

    /**
     * @see EnhancedBookmarksBridge#addSearchObserver(SearchServiceObserver)
     */
    public void addSearchObserver(SearchServiceObserver observer) {
        mEnhancedBookmarksBridge.addSearchObserver(observer);
    }

    /**
     * @see EnhancedBookmarksBridge#removeSearchObserver(SearchServiceObserver)
     */
    public void removeSearchObserver(SearchServiceObserver observer) {
        mEnhancedBookmarksBridge.removeSearchObserver(observer);
    }

    /**
     * @see BookmarksBridge#loadEmptyPartnerBookmarkShimForTesting()
     */
    @VisibleForTesting
    public void loadEmptyPartnerBookmarkShimForTesting() {
        mBookmarksBridge.loadEmptyPartnerBookmarkShimForTesting();
    }
}

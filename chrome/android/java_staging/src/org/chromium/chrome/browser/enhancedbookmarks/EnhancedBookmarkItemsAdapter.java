// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enhancedbookmarks;

import android.content.Context;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.view.ViewGroup;
import android.widget.Checkable;

import com.google.android.apps.chrome.R;

import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.chrome.browser.BookmarksBridge.BookmarkItem;
import org.chromium.chrome.browser.BookmarksBridge.BookmarkModelObserver;
import org.chromium.chrome.browser.enhanced_bookmarks.EnhancedBookmarksBridge.FiltersObserver;
import org.chromium.chrome.browser.enhancedbookmarks.EnhancedBookmarkManager.UIState;
import org.chromium.chrome.browser.enhancedbookmarks.EnhancedBookmarkPromoHeader.PromoHeaderShowingChangeListener;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.components.bookmarks.BookmarkId;

import java.util.ArrayList;
import java.util.List;

/**
 * BaseAdapter for EnhancedBookmarkItemsContainer. It manages bookmarks to list there.
 */
class EnhancedBookmarkItemsAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> implements
        EnhancedBookmarkUIObserver, PromoHeaderShowingChangeListener {

    /**
     * An abstraction for the common functionalities for {@link EnhancedBookmarkFolder} and
     * {@link EnhancedBookmarkItem}
     */
    public interface BookmarkGrid extends OnClickListener, Checkable {
        /**
         * Sets the bookmarkId the object is holding. Corresponding UI changes might occur.
         */
        public void setBookmarkId(BookmarkId id);

        /**
         * @return The bookmark that the object is holding.
         */
        public BookmarkId getBookmarkId();
    }

    public static final int PROMO_HEADER_GRID = 0;
    public static final int FOLDER_VIEW_GRID = 1;
    public static final int PADDING_VIEW = 2;
    public static final int BOOKMARK_VIEW_GRID = 3;

    // Have different view types for list and grid modes, in order to force the recycler not reuse
    // the same tiles across the two modes.
    public static final int PROMO_HEADER_LIST = 4;
    public static final int FOLDER_VIEW_LIST = 5;
    public static final int DIVIDER_LIST = 6;
    public static final int BOOKMARK_VIEW_LIST = 7;

    private EnhancedBookmarkDelegate mDelegate;
    private Context mContext;
    private EnhancedBookmarkPromoHeader mPromoHeaderManager;
    private ItemFactory mItemFactory;
    private List<BookmarkId> mFolders = new ArrayList<BookmarkId>();
    private List<BookmarkId> mBookmarks = new ArrayList<BookmarkId>();

    // These are used to track placeholder invisible view between folder items and bookmark items,
    // to align the beginning of the bookmark items to be the first column.
    // In list mode, this variable represents the presence of the divider, either 1 or 0.
    private int mEmptyViewCount;
    private int mColumnCount = 1;

    private BookmarkModelObserver mBookmarkModelObserver = new BookmarkModelObserver() {
        @Override
        public void bookmarkNodeChanged(BookmarkItem node) {
            int position = getPositionForBookmark(node.getId());
            if (position >= 0) notifyItemChanged(position);
        }

        @Override
        public void bookmarkNodeRemoved(BookmarkItem parent, int oldIndex, BookmarkItem node,
                boolean isDoingExtensiveChanges) {
            if (node.isFolder()) {
                mDelegate.notifyStateChange(EnhancedBookmarkItemsAdapter.this);
            } else {
                int deletedPosition = getPositionForBookmark(node.getId());
                if (deletedPosition >= 0) {
                    removeItem(deletedPosition);
                }
            }
        }

        @Override
        public void bookmarkModelChanged() {
            mDelegate.notifyStateChange(EnhancedBookmarkItemsAdapter.this);
        }
    };

    private FiltersObserver mFiltersObserver = new FiltersObserver() {
        @Override
        public void onFiltersChanged() {
            if (mDelegate.getCurrentState() == UIState.STATE_FILTER) {
                mDelegate.notifyStateChange(EnhancedBookmarkItemsAdapter.this);
            }
        }
    };

    EnhancedBookmarkItemsAdapter(Context context) {
        mContext = context;
    }

    /**
     * @return Whether the first item is header.
     */
    boolean isHeader(int position) {
        int type = getItemViewType(position);
        return type == PROMO_HEADER_GRID || type == PROMO_HEADER_LIST;
    }

    /**
     * Set folders and bookmarks to show.
     * @param folders This can be null if there is no folders to show.
     */
    void setBookmarks(List<BookmarkId> folders, List<BookmarkId> bookmarks) {
        if (folders == null) folders = new ArrayList<BookmarkId>();
        mFolders = folders;
        mBookmarks = bookmarks;

        // TODO(kkimlabs): Animation is disabled due to a performance issue on bookmark undo.
        //                 http://crbug.com/484174
        updateDataset();
    }

    /**
     * Set number of columns. This information will be used to align the starting position of
     * bookmarks to be the beginning of the row.
     *
     * This is a hacky responsibility violation, since adapter shouldn't care about the way items
     * are presented in the UI. However, there is no other easy way to do this correctly with
     * GridView.
     */
    void setNumColumns(int columnNumber) {
        if (mColumnCount == columnNumber) return;
        mColumnCount = columnNumber;
        updateDataset();
    }

    /**
     * If called, recycler view will be brutally reset and no animation will be shown.
     */
    void updateDataset() {
        updateEmptyViewCount();
        notifyDataSetChanged();
    }

    private void updateEmptyViewCount() {
        if (mDelegate.isListModeEnabled()) mEmptyViewCount = mFolders.size() > 0 ? 1 : 0;
        else mEmptyViewCount = MathUtils.positiveModulo(-mFolders.size(), mColumnCount);
    }

    private int getHeaderItemsCount() {
        // In listview a promo header carries a divider below it.
        if (mPromoHeaderManager.isShowing()) {
            return mDelegate.isListModeEnabled() ? 2 : 1;
        }
        return 0;
    }

    @Override
    public int getItemCount() {
        return getHeaderItemsCount() + mFolders.size() + mEmptyViewCount + mBookmarks.size();
    }

    /**
     * @return The position of the given bookmark in adapter. Will return -1 if not found.
     */
    int getPositionForBookmark(BookmarkId bookmark) {
        assert bookmark != null;
        int position = -1;
        for (int i = 0; i < getItemCount(); i++) {
            if (bookmark.equals(getItem(i))) {
                position = i;
                break;
            }
        }
        return position;
    }

    BookmarkId getItem(int position) {
        switch(getItemViewType(position)) {
            case PROMO_HEADER_GRID:
            case PROMO_HEADER_LIST:
                return null;
            case FOLDER_VIEW_GRID:
            case FOLDER_VIEW_LIST:
                return mFolders.get(position - getHeaderItemsCount());
            case PADDING_VIEW:
            case DIVIDER_LIST:
                return null;
            case BOOKMARK_VIEW_GRID:
            case BOOKMARK_VIEW_LIST:
                return mBookmarks.get(
                        position - getHeaderItemsCount() - mFolders.size() - mEmptyViewCount);
            default:
                assert false;
                return null;
        }
    }

    void removeItem(int position) {
        switch(getItemViewType(position)) {
            case PROMO_HEADER_GRID:
            case PROMO_HEADER_LIST:
                assert false : "Promo header remove should be handled in PromoHeaderManager.";
                break;
            case FOLDER_VIEW_GRID:
            case FOLDER_VIEW_LIST:
                mFolders.remove(position - getHeaderItemsCount());
                notifyItemRemoved(position);
                break;
            case PADDING_VIEW:
            case DIVIDER_LIST:
                assert false : "Cannot remove a padding view or a divider";
                break;
            case BOOKMARK_VIEW_GRID:
            case BOOKMARK_VIEW_LIST:
                mBookmarks.remove(
                        position
                        - getHeaderItemsCount()
                        - mFolders.size()
                        - mEmptyViewCount);
                notifyItemRemoved(position);
                break;
            default:
                assert false;
        }
    }

    @Override
    public int getItemViewType(int position) {
        int i = position;

        if (mPromoHeaderManager.isShowing()) {
            if (mDelegate.isListModeEnabled()) {
                if (i == 0) return PROMO_HEADER_LIST;
                if (i == 1) return DIVIDER_LIST;
            } else {
                if (i == 0) return PROMO_HEADER_GRID;
            }
        }
        i -= getHeaderItemsCount();

        if (i < mFolders.size()) {
            return mDelegate.isListModeEnabled() ? FOLDER_VIEW_LIST : FOLDER_VIEW_GRID;
        }
        i -= mFolders.size();

        if (i < mEmptyViewCount) {
            if (mDelegate.isListModeEnabled()) return DIVIDER_LIST;
            else return PADDING_VIEW;
        }
        i -= mEmptyViewCount;

        if (i < mBookmarks.size()) {
            return mDelegate.isListModeEnabled() ? BOOKMARK_VIEW_LIST : BOOKMARK_VIEW_GRID;
        }

        assert false : "Invalid position requested";
        return -1;
    }

    @Override
    public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        switch (viewType) {
            case PROMO_HEADER_GRID:
            case PROMO_HEADER_LIST:
                return mPromoHeaderManager.createHolder(parent, mDelegate.isListModeEnabled());
            case FOLDER_VIEW_GRID:
            case FOLDER_VIEW_LIST:
                EnhancedBookmarkFolder folder = mItemFactory.createBookmarkFolder(parent);
                folder.setDelegate(mDelegate);
                return new ItemViewHolder(folder, mDelegate);
            case PADDING_VIEW:
                // Padding views are empty place holders that will be set invisible.
                return new ViewHolder(new View(parent.getContext())) {};
            case DIVIDER_LIST:
                return new ViewHolder(LayoutInflater.from(parent.getContext()).inflate(
                        R.layout.eb_list_divider, parent, false)) {};
            case BOOKMARK_VIEW_GRID:
            case BOOKMARK_VIEW_LIST:
                EnhancedBookmarkItem item = mItemFactory.createBookmarkItem(parent);
                item.onEnhancedBookmarkDelegateInitialized(mDelegate);
                return new ItemViewHolder(item, mDelegate);
            default:
                assert false;
                return null;
        }
    }

    @SuppressFBWarnings("BC_UNCONFIRMED_CAST")
    @Override
    public void onBindViewHolder(ViewHolder holder, int position) {
        BookmarkId id = getItem(position);
        switch (getItemViewType(position)) {
            case PROMO_HEADER_GRID:
            case PROMO_HEADER_LIST:
                break;
            case FOLDER_VIEW_GRID:
            case FOLDER_VIEW_LIST:
                ((ItemViewHolder) holder).setBookmarkId(id);
                break;
            case PADDING_VIEW:
            case DIVIDER_LIST:
                break;
            case BOOKMARK_VIEW_GRID:
            case BOOKMARK_VIEW_LIST:
                ((ItemViewHolder) holder).setBookmarkId(id);
                break;
            default:
                assert false : "View type not supported!";
        }
    }

    // PromoHeaderShowingChangeListener implementation.

    @Override
    public void onPromoHeaderShowingChanged(boolean isShowing) {
        if (isShowing) {
            notifyItemInserted(0);
        } else {
            // TODO(kkimlabs): Ideally we want to animate by |notifyItemRemoved| but the default
            // animation looks broken for this promo header for some reason.
            notifyDataSetChanged();
        }
    }

    // EnhancedBookmarkUIObserver implementations.

    @Override
    public void onEnhancedBookmarkDelegateInitialized(EnhancedBookmarkDelegate delegate) {
        mDelegate = delegate;
        mDelegate.addUIObserver(this);
        mDelegate.getModel().addModelObserver(mBookmarkModelObserver);
        mDelegate.getModel().addFiltersObserver(mFiltersObserver);

        mItemFactory = mDelegate.isListModeEnabled()
                ? new ListItemFactory() : new GridItemFactory();

        mPromoHeaderManager = new EnhancedBookmarkPromoHeader(mContext, this);
    }

    @Override
    public void onDestroy() {
        mDelegate.removeUIObserver(this);
        mDelegate.getModel().removeModelObserver(mBookmarkModelObserver);
        mDelegate.getModel().removeFiltersObserver(mFiltersObserver);

        mPromoHeaderManager.destroy();
    }

    @Override
    public void onAllBookmarksStateSet() {
        setBookmarks(null, mDelegate.getModel().getAllBookmarkIDsOrderedByCreationDate());
    }

    @Override
    public void onFolderStateSet(BookmarkId folder) {
        setBookmarks(mDelegate.getModel().getChildIDs(folder, true, false),
                mDelegate.getModel().getChildIDs(folder, false, true));
    }

    @Override
    public void onFilterStateSet(String filter) {
        setBookmarks(null, mDelegate.getModel().getBookmarksForFilter(filter));
    }

    @Override
    public void onSelectionStateChange(List<BookmarkId> selectedBookmarks) {}

    @Override
    public void onListModeChange(boolean isListModeEnabled) {
        if (isListModeEnabled) {
            setNumColumns(1);
            mItemFactory = new EnhancedBookmarkItemsAdapter.ListItemFactory();
        } else {
            mItemFactory = new EnhancedBookmarkItemsAdapter.GridItemFactory();
        }
        updateDataset();
    }

    private static class ItemViewHolder extends RecyclerView.ViewHolder implements OnClickListener,
            OnLongClickListener {
        private EnhancedBookmarkDelegate mDelegate;
        private BookmarkGrid mGrid;

        public ItemViewHolder(View view, EnhancedBookmarkDelegate delegate) {
            super(view);
            mGrid = (BookmarkGrid) view;
            mDelegate = delegate;
            view.setOnClickListener(this);
            view.setOnLongClickListener(this);
        }

        public void setBookmarkId(BookmarkId id) {
            mGrid.setBookmarkId(id);
            mGrid.setChecked(mDelegate.isBookmarkSelected(mGrid.getBookmarkId()));
        }

        @Override
        public boolean onLongClick(View v) {
            mGrid.setChecked(mDelegate.toggleSelectionForBookmark(mGrid.getBookmarkId()));
            return true;
        }

        @Override
        public void onClick(View v) {
            if (mDelegate.isSelectionEnabled()) onLongClick(v);
            else mGrid.onClick(itemView);
        }
    }

    /**
     * Factory interface to get the correct views to show.
     */
    private interface ItemFactory {
        EnhancedBookmarkItem createBookmarkItem(ViewGroup parent);
        EnhancedBookmarkFolder createBookmarkFolder(ViewGroup parent);
    }

    private static class GridItemFactory implements ItemFactory {
        @Override
        public EnhancedBookmarkItem createBookmarkItem(ViewGroup parent) {
            return (EnhancedBookmarkItem) LayoutInflater.from(parent.getContext()).inflate(
                    R.layout.eb_grid_item, parent, false);
        }

        @Override
        public EnhancedBookmarkFolder createBookmarkFolder(ViewGroup parent) {
            return (EnhancedBookmarkFolder) LayoutInflater.from(
                    parent.getContext()).inflate(R.layout.eb_grid_folder, parent, false);
        }
    }

    private static class ListItemFactory implements ItemFactory {
        @Override
        public EnhancedBookmarkItem createBookmarkItem(ViewGroup parent) {
            return (EnhancedBookmarkItem) LayoutInflater.from(parent.getContext()).inflate(
                    R.layout.eb_list_item, parent, false);
        }

        @Override
        public EnhancedBookmarkFolder createBookmarkFolder(ViewGroup parent) {
            return (EnhancedBookmarkFolder) LayoutInflater.from(
                    parent.getContext()).inflate(R.layout.eb_list_folder, parent, false);
        }
    }
}

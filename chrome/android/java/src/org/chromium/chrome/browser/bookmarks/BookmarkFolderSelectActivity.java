// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.support.v7.widget.Toolbar;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.ListView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.SynchronousInitializationActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkModelObserver;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.widget.TintedDrawable;
import org.chromium.chrome.browser.widget.TintedImageButton;
import org.chromium.chrome.browser.widget.TintedImageView;
import org.chromium.components.bookmarks.BookmarkId;

import java.util.ArrayList;
import java.util.List;

/**
 * Dialog for moving bookmarks from one folder to another. A list of folders are shown and the
 * hierarchy of bookmark model is presented by indentation of list items. This dialog can be shown
 * in two cases. One is when user choose to move an existing bookmark to a new folder. The other is
 * when user creates a new folder/bookmark, they can choose which parent the new folder/bookmark
 * belong to.
 * <p>
 * Note this fragment will not be restarted by OS. It will be dismissed if chrome is killed in
 * background.
 */
public class BookmarkFolderSelectActivity extends SynchronousInitializationActivity implements
        AdapterView.OnItemClickListener {
    static final String
            INTENT_SELECTED_FOLDER = "BookmarkFolderSelectActivity.selectedFolder";
    static final String
            INTENT_IS_CREATING_FOLDER = "BookmarkFolderSelectActivity.isCreatingFolder";
    static final String
            INTENT_BOOKMARKS_TO_MOVE = "BookmarkFolderSelectActivity.bookmarksToMove";
    static final int CREATE_FOLDER_REQUEST_CODE = 13;

    private BookmarkModel mModel;
    private boolean mIsCreatingFolder;
    private List<BookmarkId> mBookmarksToMove;
    private BookmarkId mParentId;
    private FolderListAdapter mBookmarkIdsAdapter;
    private ListView mBookmarkIdsList;

    private BookmarkModelObserver mBookmarkModelObserver = new BookmarkModelObserver() {
        @Override
        public void bookmarkModelChanged() {
            updateFolderList();
        }

        @Override
        public void bookmarkNodeRemoved(BookmarkItem parent, int oldIndex, BookmarkItem node,
                boolean isDoingExtensiveChanges) {
            if (mBookmarksToMove.contains(node.getId())) {
                mBookmarksToMove.remove(node.getId());
                if (mBookmarksToMove.isEmpty()) {
                    finish();
                    return;
                }
            } else if (node.isFolder()) {
                updateFolderList();
            }
        }
    };

    /**
     * Starts a select folder activity.
     */
    public static void startFolderSelectActivity(Context context, BookmarkId... bookmarks) {
        assert bookmarks.length > 0;
        Intent intent = new Intent(context, BookmarkFolderSelectActivity.class);
        intent.putExtra(INTENT_IS_CREATING_FOLDER, false);
        ArrayList<String> bookmarkStrings = new ArrayList<>(bookmarks.length);
        for (BookmarkId id : bookmarks) {
            bookmarkStrings.add(id.toString());
        }
        intent.putStringArrayListExtra(INTENT_BOOKMARKS_TO_MOVE, bookmarkStrings);
        context.startActivity(intent);
    }

    /**
     * Starts a select folder activity for the new folder that is about to be created. This method
     * is only supposed to be called by {@link BookmarkAddEditFolderActivity}
     */
    public static void startNewFolderSelectActivity(
            BookmarkAddEditFolderActivity activity, List<BookmarkId> bookmarks) {
        assert bookmarks.size() > 0;
        Intent intent = new Intent(activity, BookmarkFolderSelectActivity.class);
        intent.putExtra(INTENT_IS_CREATING_FOLDER, true);
        ArrayList<String> bookmarkStrings = new ArrayList<>(bookmarks.size());
        for (BookmarkId id : bookmarks) {
            bookmarkStrings.add(id.toString());
        }
        intent.putStringArrayListExtra(INTENT_BOOKMARKS_TO_MOVE, bookmarkStrings);
        activity.startActivityForResult(intent,
                BookmarkAddEditFolderActivity.PARENT_FOLDER_REQUEST_CODE);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mModel = new BookmarkModel();
        mModel.addObserver(mBookmarkModelObserver);
        List<String> stringList = getIntent().getStringArrayListExtra(INTENT_BOOKMARKS_TO_MOVE);
        mBookmarksToMove = new ArrayList<>(stringList.size());
        for (String string : stringList) {
            BookmarkId bookmarkId = BookmarkId.getBookmarkIdFromString(string);
            if (mModel.doesBookmarkExist(bookmarkId)) {
                mBookmarksToMove.add(bookmarkId);
            }
        }
        if (mBookmarksToMove.isEmpty()) {
            finish();
            return;
        }

        mIsCreatingFolder = getIntent().getBooleanExtra(INTENT_IS_CREATING_FOLDER, false);
        if (mIsCreatingFolder) {
            mParentId = mModel.getMobileFolderId();
        } else {
            mParentId = mModel.getBookmarkById(mBookmarksToMove.get(0))
                    .getParentId();
        }

        setContentView(R.layout.bookmark_folder_select_activity);
        mBookmarkIdsList = (ListView) findViewById(R.id.bookmark_folder_list);
        mBookmarkIdsList.setOnItemClickListener(this);
        mBookmarkIdsAdapter = new FolderListAdapter(this);
        mBookmarkIdsList.setAdapter(mBookmarkIdsAdapter);

        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);
        getSupportActionBar().setDisplayHomeAsUpEnabled(true);

        updateFolderList();

        if (!FeatureUtilities.isChromeHomeEnabled()) {
            findViewById(R.id.shadow).setVisibility(View.VISIBLE);
            toolbar.setTitleTextAppearance(toolbar.getContext(), R.style.BlackHeadline2);
            toolbar.setBackgroundColor(
                    ApiCompatibilityUtils.getColor(getResources(), R.color.modern_primary_color));
        }
    }

    private void updateFolderList() {
        List<BookmarkId> folderList = new ArrayList<>();
        List<Integer> depthList = new ArrayList<>();
        mModel.getMoveDestinations(folderList, depthList, mBookmarksToMove);
        List<FolderListEntry> entryList = new ArrayList<>(folderList.size() + 3);

        if (!mIsCreatingFolder) {
            entryList.add(new FolderListEntry(null, 0,
                    getString(R.string.bookmark_add_folder), false,
                    FolderListEntry.TYPE_NEW_FOLDER));
        }

        for (int i = 0; i < folderList.size(); i++) {
            BookmarkId folder = folderList.get(i);

            if (!mModel.isFolderVisible(folder)) continue;

            String title = mModel.getBookmarkById(folder).getTitle();
            entryList.add(new FolderListEntry(folder, depthList.get(i), title,
                    folder.equals(mParentId), FolderListEntry.TYPE_NORMAL));
        }

        mBookmarkIdsAdapter.setEntryList(entryList);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            onBackPressed();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        mModel.removeObserver(mBookmarkModelObserver);
        mModel.destroy();
        mModel = null;
    }

    /**
     * Moves bookmark from original parent to selected folder. In creation mode, directly add the
     * new bookmark to selected folder instead of moving.
     */
    @Override
    public void onItemClick(AdapterView<?> adapter, View view, int position, long id) {
        FolderListEntry entry = (FolderListEntry) adapter.getItemAtPosition(position);
        if (mIsCreatingFolder) {
            BookmarkId selectedFolder = null;
            if (entry.mType == FolderListEntry.TYPE_NORMAL) {
                selectedFolder = entry.mId;
            } else {
                assert false : "New folder items should not be clickable in creating mode";
            }
            Intent intent = new Intent();
            intent.putExtra(INTENT_SELECTED_FOLDER, selectedFolder.toString());
            setResult(RESULT_OK, intent);
            finish();
        } else if (entry.mType == FolderListEntry.TYPE_NEW_FOLDER) {
            BookmarkAddEditFolderActivity.startAddFolderActivity(this, mBookmarksToMove);
        } else if (entry.mType == FolderListEntry.TYPE_NORMAL) {
            mModel.moveBookmarks(mBookmarksToMove, entry.mId);
            BookmarkUtils.setLastUsedParent(this, entry.mId);
            finish();
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        assert !mIsCreatingFolder;
        if (requestCode == CREATE_FOLDER_REQUEST_CODE && resultCode == RESULT_OK) {
            BookmarkId createdBookmark = BookmarkId.getBookmarkIdFromString(data.getStringExtra(
                    BookmarkAddEditFolderActivity.INTENT_CREATED_BOOKMARK));
            mModel.moveBookmarks(mBookmarksToMove, createdBookmark);
            BookmarkUtils.setLastUsedParent(this, createdBookmark);
            finish();
        }
    }

    /**
     * Data object representing a folder entry used in FolderListAdapter.
     */
    private static class FolderListEntry {
        public static final int TYPE_NEW_FOLDER = 0;
        public static final int TYPE_NORMAL = 1;

        BookmarkId mId;
        String mTitle;
        int mDepth;
        boolean mIsSelected;
        int mType;

        public FolderListEntry(BookmarkId bookmarkId, int depth, String title, boolean isSelected,
                int type) {
            assert type == TYPE_NEW_FOLDER || type == TYPE_NORMAL;
            mDepth = depth;
            mId = bookmarkId;
            mTitle = title;
            mIsSelected = isSelected;
            mType = type;
        }
    }

    private static class FolderListAdapter extends BaseAdapter {
        // The maximum depth that will be indented. Folders with a depth greater
        // than this will all appear at this same depth.
        private static final int MAX_FOLDER_DEPTH = 5;

        private final int mBasePadding;
        private final int mPaddingIncrement;

        List<FolderListEntry> mEntryList = new ArrayList<>();

        public FolderListAdapter(Context context) {
            mBasePadding = context.getResources()
                    .getDimensionPixelSize(R.dimen.bookmark_folder_item_left);
            mPaddingIncrement = mBasePadding * 2;
        }

        @Override
        public int getCount() {
            return mEntryList.size();
        }

        @Override
        public FolderListEntry getItem(int position) {
            return mEntryList.get(position);
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        /**
         * There are 2 types of entries: new folder and normal.
         */
        @Override
        public int getViewTypeCount() {
            return 2;
        }

        @Override
        public int getItemViewType(int position) {
            FolderListEntry entry = getItem(position);
            return entry.mType;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            final FolderListEntry entry = getItem(position);
            if (convertView != null && entry.mType != FolderListEntry.TYPE_NORMAL) {
                return convertView;
            }
            if (convertView == null) {
                convertView = LayoutInflater.from(parent.getContext()).inflate(
                        R.layout.bookmark_folder_select_item, parent, false);
            }
            TextView textView = (TextView) convertView.findViewById(R.id.title);
            textView.setText(entry.mTitle);
            convertView.findViewById(R.id.description).setVisibility(View.GONE);

            setUpIcons(entry, convertView);
            setUpPadding(entry, convertView);

            return convertView;
        }

        void setEntryList(List<FolderListEntry> entryList) {
            mEntryList = entryList;
            notifyDataSetChanged();
        }

        /**
         * Sets compound drawables (icons) for different kinds of list entries,
         * i.e. New Folder, Normal and Selected.
         */
        private void setUpIcons(FolderListEntry entry, View view) {
            TintedImageView startIcon = view.findViewById(R.id.icon_view);
            TintedImageButton endIcon = view.findViewById(R.id.selected_view);

            Drawable iconDrawable;
            if (entry.mType == FolderListEntry.TYPE_NORMAL) {
                iconDrawable = BookmarkUtils.getFolderIcon(view.getResources());
            } else {
                // For new folder, start_icon is different.
                VectorDrawableCompat vectorDrawable = VectorDrawableCompat.create(
                        view.getResources(), R.drawable.ic_add, view.getContext().getTheme());
                vectorDrawable.setTintList(ApiCompatibilityUtils.getColorStateList(
                        view.getResources(), R.color.dark_mode_tint));
                iconDrawable = vectorDrawable;
            }

            if (FeatureUtilities.isChromeHomeEnabled()) {
                startIcon.setBackgroundResource(R.drawable.list_item_icon_modern_bg);
                startIcon.setImageDrawable(entry.mIsSelected
                                ? TintedDrawable.constructTintedDrawable(view.getResources(),
                                          R.drawable.ic_check_googblue_24dp,
                                          R.color.white_mode_tint)
                                : iconDrawable);
                startIcon.getBackground().setLevel(entry.mIsSelected
                                ? view.getResources().getInteger(R.integer.list_item_level_selected)
                                : view.getResources().getInteger(
                                          R.integer.list_item_level_default));
                endIcon.setVisibility(View.GONE);
            } else {
                // Selected entry has an end_icon, a blue check mark.
                startIcon.setImageDrawable(iconDrawable);
                endIcon.setVisibility(entry.mIsSelected ? View.VISIBLE : View.GONE);
            }
        }

        /**
         * Sets up padding for the entry
         */
        private void setUpPadding(FolderListEntry entry, View view) {
            int paddingStart = mBasePadding + Math.min(entry.mDepth, MAX_FOLDER_DEPTH)
                    * mPaddingIncrement;
            View endIcon = view.findViewById(R.id.selected_view);
            ApiCompatibilityUtils.setPaddingRelative(view, paddingStart, view.getPaddingTop(),
                    (endIcon.getVisibility() == View.VISIBLE) ? 0 : mBasePadding,
                    view.getPaddingBottom());
        }
    }
}

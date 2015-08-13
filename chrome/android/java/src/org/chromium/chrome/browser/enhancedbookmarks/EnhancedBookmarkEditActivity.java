// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enhancedbookmarks;

import android.os.Bundle;
import android.support.v7.widget.Toolbar;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.TextView;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.BookmarksBridge.BookmarkItem;
import org.chromium.chrome.browser.BookmarksBridge.BookmarkModelObserver;
import org.chromium.chrome.browser.UrlUtilities;
import org.chromium.chrome.browser.widget.EmptyAlertEditText;
import org.chromium.chrome.browser.widget.TintedDrawable;
import org.chromium.components.bookmarks.BookmarkId;

/**
 * The activity that enables the user to modify the title, url and parent folder of a bookmark.
 */
public class EnhancedBookmarkEditActivity extends EnhancedBookmarkActivityBase {
    /** The intent extra specifying the ID of the bookmark to be edited. */
    public static final String INTENT_BOOKMARK_ID = "EnhancedBookmarkEditActivity.BookmarkId";

    private static final String TAG = "cr.BookmarkEdit";

    private EnhancedBookmarksModel mEnhancedBookmarksModel;
    private BookmarkId mBookmarkId;
    private EmptyAlertEditText mTitleEditText;
    private EmptyAlertEditText mUrlEditText;
    private TextView mFolderTextView;

    private MenuItem mDeleteButton;

    private BookmarkModelObserver mBookmarkModelObserver = new BookmarkModelObserver() {
        @Override
        public void bookmarkNodeRemoved(BookmarkItem parent, int oldIndex, BookmarkItem node,
                boolean isDoingExtensiveChanges) {
            if (mBookmarkId.equals(node.getId())) {
                finish();
            }
        }

        @Override
        public void bookmarkNodeMoved(BookmarkItem oldParent, int oldIndex, BookmarkItem newParent,
                int newIndex) {
            BookmarkId movedBookmark = mEnhancedBookmarksModel.getChildAt(newParent.getId(),
                    newIndex);
            if (movedBookmark.equals(mBookmarkId)) {
                mFolderTextView.setText(newParent.getTitle());
            }
        }

        @Override
        public void bookmarkNodeChanged(BookmarkItem node) {
            if (mBookmarkId.equals(node.getId()) || node.getId().equals(
                    mEnhancedBookmarksModel.getBookmarkById(mBookmarkId).getParentId())) {
                updateViewContent();
            }
        }

        @Override
        public void bookmarkModelChanged() {
            if (!mEnhancedBookmarksModel.doesBookmarkExist(mBookmarkId)) {
                Log.wtf(TAG, "The bookmark was deleted somehow during bookmarkModelChange!",
                        new Exception(TAG));
                finish();
            }
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EnhancedBookmarkUtils.setTaskDescriptionInDocumentMode(this,
                getString(R.string.edit_bookmark));
        mEnhancedBookmarksModel = new EnhancedBookmarksModel();
        mBookmarkId = BookmarkId.getBookmarkIdFromString(
                getIntent().getStringExtra(INTENT_BOOKMARK_ID));
        mEnhancedBookmarksModel.addObserver(mBookmarkModelObserver);

        setContentView(R.layout.eb_edit);
        mTitleEditText = (EmptyAlertEditText) findViewById(R.id.title_text);
        mUrlEditText = (EmptyAlertEditText) findViewById(R.id.url_text);
        mFolderTextView = (TextView) findViewById(R.id.folder_text);
        mFolderTextView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                EnhancedBookmarkFolderSelectActivity.startFolderSelectActivity(
                        EnhancedBookmarkEditActivity.this, mBookmarkId);
            }
        });
        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);
        getSupportActionBar().setDisplayHomeAsUpEnabled(true);

        updateViewContent();
    }

    private void updateViewContent() {
        BookmarkItem bookmarkItem = mEnhancedBookmarksModel.getBookmarkById(mBookmarkId);
        mTitleEditText.setText(bookmarkItem.getTitle());
        mUrlEditText.setText(bookmarkItem.getUrl());
        mUrlEditText.setEnabled(bookmarkItem.isUrlEditable());
        mFolderTextView.setText(
                mEnhancedBookmarksModel.getBookmarkTitle(bookmarkItem.getParentId()));
        mFolderTextView.setEnabled(bookmarkItem.isMovable());
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        mDeleteButton = menu.add(R.string.enhanced_bookmark_action_bar_delete)
                .setIcon(TintedDrawable.constructTintedDrawable(
                        getResources(), R.drawable.btn_trash))
                .setShowAsActionFlags(MenuItem.SHOW_AS_ACTION_IF_ROOM);

        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item == mDeleteButton) {
            // Log added for detecting delete button double clicking.
            Log.i(TAG, "Delete button pressed by user! isFinishing() == " + isFinishing());

            mEnhancedBookmarksModel.deleteBookmark(mBookmarkId);
            finish();
            return true;
        } else if (item.getItemId() == android.R.id.home) {
            onBackPressed();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onBackPressed() {
        if (isFinishing()) return;

        String newTitle = mTitleEditText.getTrimmedText();
        String newUrl = mUrlEditText.getTrimmedText();
        newUrl = UrlUtilities.fixupUrl(newUrl);
        if (newUrl == null) newUrl = "";
        mUrlEditText.setText(newUrl);

        if (!mTitleEditText.validate() || !mUrlEditText.validate()) return;

        mEnhancedBookmarksModel.setBookmarkTitle(mBookmarkId, newTitle);
        mEnhancedBookmarksModel.setBookmarkUrl(mBookmarkId, newUrl);
        super.onBackPressed();
    }

    @Override
    protected void onDestroy() {
        mEnhancedBookmarksModel.removeObserver(mBookmarkModelObserver);
        mEnhancedBookmarksModel.destroy();
        mEnhancedBookmarksModel = null;
        super.onDestroy();
    }
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enhancedbookmarks;

import android.content.Intent;
import android.os.Bundle;
import android.support.v7.widget.Toolbar;
import android.text.TextUtils;
import android.text.format.Formatter;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmark.BookmarksBridge.BookmarkItem;
import org.chromium.chrome.browser.bookmark.BookmarksBridge.BookmarkModelObserver;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.DeletePageCallback;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.OfflinePageModelObserver;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.SavePageCallback;
import org.chromium.chrome.browser.offlinepages.OfflinePageItem;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.chrome.browser.widget.EmptyAlertEditText;
import org.chromium.chrome.browser.widget.TintedDrawable;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.content_public.browser.WebContents;

/**
 * The activity that enables the user to modify the title, url and parent folder of a bookmark.
 */
public class EnhancedBookmarkEditActivity extends EnhancedBookmarkActivityBase {
    /** The intent extra specifying the ID of the bookmark to be edited. */
    public static final String INTENT_BOOKMARK_ID = "EnhancedBookmarkEditActivity.BookmarkId";
    public static final String INTENT_WEB_CONTENTS = "EnhancedBookmarkEditActivity.WebContents";

    private static final String TAG = "BookmarkEdit";

    private enum OfflineButtonType {
        NONE,
        SAVE,
        REMOVE,
        VISIT,
    }

    private EnhancedBookmarksModel mModel;
    private BookmarkId mBookmarkId;
    private EmptyAlertEditText mTitleEditText;
    private EmptyAlertEditText mUrlEditText;
    private TextView mFolderTextView;

    private WebContents mWebContents;

    private MenuItem mDeleteButton;

    private OfflineButtonType mOfflineButtonType = OfflineButtonType.NONE;
    private OfflinePageModelObserver mOfflinePageModelObserver;

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
            BookmarkId movedBookmark = mModel.getChildAt(newParent.getId(),
                    newIndex);
            if (movedBookmark.equals(mBookmarkId)) {
                mFolderTextView.setText(newParent.getTitle());
            }
        }

        @Override
        public void bookmarkNodeChanged(BookmarkItem node) {
            if (mBookmarkId.equals(node.getId()) || node.getId().equals(
                    mModel.getBookmarkById(mBookmarkId).getParentId())) {
                updateViewContent();
            }
        }

        @Override
        public void bookmarkModelChanged() {
            if (mModel.doesBookmarkExist(mBookmarkId)) {
                updateViewContent();
            } else {
                Log.wtf(TAG, "The bookmark was deleted somehow during bookmarkModelChange!",
                        new Exception(TAG));
                finish();
            }
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        int title = OfflinePageUtils.getStringId(R.string.edit_bookmark);
        setTitle(title);
        EnhancedBookmarkUtils.setTaskDescriptionInDocumentMode(this, getString(title));
        mModel = new EnhancedBookmarksModel();
        mBookmarkId = BookmarkId.getBookmarkIdFromString(
                getIntent().getStringExtra(INTENT_BOOKMARK_ID));
        mModel.addObserver(mBookmarkModelObserver);
        BookmarkItem item = mModel.getBookmarkById(mBookmarkId);
        if (!mModel.doesBookmarkExist(mBookmarkId) || item == null) {
            finish();
            return;
        }

        setContentView(R.layout.eb_edit);
        mTitleEditText = (EmptyAlertEditText) findViewById(R.id.title_text);
        mFolderTextView = (TextView) findViewById(R.id.folder_text);
        mUrlEditText = (EmptyAlertEditText) findViewById(R.id.url_text);

        mFolderTextView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                EnhancedBookmarkFolderSelectActivity.startFolderSelectActivity(
                        EnhancedBookmarkEditActivity.this, mBookmarkId);
            }
        });

        if (OfflinePageBridge.isEnabled() && OfflinePageBridge.canSavePage(
                mModel.getBookmarkById(mBookmarkId).getUrl())) {
            mOfflinePageModelObserver = new OfflinePageModelObserver() {
                @Override
                public void offlinePageDeleted(BookmarkId bookmarkId) {
                    if (mBookmarkId.equals(bookmarkId)) {
                        updateOfflineSection();
                    }
                }
            };

            mModel.getOfflinePageBridge().addObserver(mOfflinePageModelObserver);
            // Make offline page section visible and find controls.
            findViewById(R.id.offline_page_group).setVisibility(View.VISIBLE);
            getIntent().setExtrasClassLoader(WebContents.class.getClassLoader());
            mWebContents = getIntent().getParcelableExtra(INTENT_WEB_CONTENTS);
            updateOfflineSection();
        }

        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);
        getSupportActionBar().setDisplayHomeAsUpEnabled(true);

        updateViewContent();
    }

    private void updateViewContent() {
        BookmarkItem bookmarkItem = mModel.getBookmarkById(mBookmarkId);

        if (!TextUtils.equals(mTitleEditText.getTrimmedText(), bookmarkItem.getTitle())) {
            mTitleEditText.setText(bookmarkItem.getTitle());
        }
        String folderTitle = mModel.getBookmarkTitle(bookmarkItem.getParentId());
        if (!TextUtils.equals(mFolderTextView.getText(), folderTitle)) {
            mFolderTextView.setText(folderTitle);
        }
        if (!TextUtils.equals(mUrlEditText.getTrimmedText(), bookmarkItem.getUrl())) {
            mUrlEditText.setText(bookmarkItem.getUrl());
        }
        mTitleEditText.setEnabled(bookmarkItem.isEditable());
        mUrlEditText.setEnabled(bookmarkItem.isUrlEditable());
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

            mModel.deleteBookmark(mBookmarkId);
            finish();
            return true;
        } else if (item.getItemId() == android.R.id.home) {
            finish();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    protected void onStop() {
        if (mModel.doesBookmarkExist(mBookmarkId)) {
            final String originalUrl =
                    mModel.getBookmarkById(mBookmarkId).getUrl();
            final String title = mTitleEditText.getTrimmedText();
            final String url = mUrlEditText.getTrimmedText();

            if (!mTitleEditText.isEmpty()) {
                mModel.setBookmarkTitle(mBookmarkId, title);
            }

            if (!mUrlEditText.isEmpty()
                    && mModel.getBookmarkById(mBookmarkId).isUrlEditable()) {
                String fixedUrl = UrlUtilities.fixupUrl(url);
                if (fixedUrl != null && !fixedUrl.equals(originalUrl)) {
                    boolean hasOfflinePage = OfflinePageBridge.isEnabled()
                            && mModel.getOfflinePageBridge()
                                    .getPageByBookmarkId(mBookmarkId) != null;
                    RecordHistogram.recordBooleanHistogram(
                            "OfflinePages.Edit.BookmarkUrlChangedForOfflinePage", hasOfflinePage);
                    mModel.setBookmarkUrl(mBookmarkId, fixedUrl);
                }
            }
        }

        super.onStop();
    }

    @Override
    protected void onDestroy() {
        recordOfflineButtonAction(false);
        if (OfflinePageBridge.isEnabled()) {
            mModel.getOfflinePageBridge().removeObserver(
                    mOfflinePageModelObserver);
        }
        mModel.removeObserver(mBookmarkModelObserver);
        mModel.destroy();
        mModel = null;
        super.onDestroy();
    }

    private void updateOfflineSection() {
        assert OfflinePageBridge.isEnabled();

        // It is possible that callback arrives after the activity was dismissed.
        // See http://crbug.com/566939
        if (mModel == null) return;

        OfflinePageBridge offlinePageBridge = mModel.getOfflinePageBridge();
        offlinePageBridge.checkOfflinePageMetadata();

        Button saveRemoveVisitButton = (Button) findViewById(R.id.offline_page_save_remove_button);
        TextView offlinePageInfoTextView = (TextView) findViewById(R.id.offline_page_info_text);

        OfflinePageItem offlinePage = offlinePageBridge.getPageByBookmarkId(mBookmarkId);
        if (offlinePage != null) {
            // Offline page exists. Show information and button to remove.
            offlinePageInfoTextView.setText(
                    getString(OfflinePageUtils.getStringId(
                                      R.string.offline_pages_as_bookmarks_offline_page_size),
                            Formatter.formatFileSize(this, offlinePage.getFileSize())));
            updateButtonToDeleteOfflinePage(saveRemoveVisitButton);
        } else if (mWebContents != null && !mWebContents.isDestroyed()
                && offlinePageBridge.canSavePage(mWebContents.getLastCommittedUrl())) {
            // Offline page is not saved, but a bookmarked page is opened. Show save button.
            offlinePageInfoTextView.setText(
                    getString(OfflinePageUtils.getStringId(R.string.bookmark_offline_page_none)));
            updateButtonToSaveOfflinePage(saveRemoveVisitButton);
        } else {
            // Offline page is not saved, and edit page was opened from the bookmarks UI, which
            // means there is no action the user can take any action - hide button.
            offlinePageInfoTextView.setText(getString(OfflinePageUtils.getStringId(
                    R.string.offline_pages_as_bookmarks_offline_page_visit)));
            updateButtonToVisitOfflinePage(saveRemoveVisitButton);
        }
    }

    private void updateButtonToDeleteOfflinePage(final Button button) {
        mOfflineButtonType = OfflineButtonType.REMOVE;
        button.setText(getString(R.string.remove));
        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                recordOfflineButtonAction(true);
                mModel.getOfflinePageBridge().deletePage(
                        mBookmarkId, new DeletePageCallback() {
                            @Override
                            public void onDeletePageDone(int deletePageResult) {
                                // TODO(fgorski): Add snackbar upon failure.
                                // Always update UI, as buttons might be disabled.
                                updateOfflineSection();
                            }
                        });
                button.setClickable(false);
            }
        });
    }

    private void updateButtonToSaveOfflinePage(final Button button) {
        mOfflineButtonType = OfflineButtonType.SAVE;
        button.setText(getString(R.string.save));
        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                recordOfflineButtonAction(true);
                mModel.getOfflinePageBridge().savePage(
                        mWebContents, mBookmarkId, new SavePageCallback() {
                            @Override
                            public void onSavePageDone(int savePageResult, String url) {
                                // TODO(fgorski): Add snackbar upon failure.
                                // Always update UI, as buttons might be disabled.
                                updateOfflineSection();
                            }
                        });
                button.setClickable(false);
            }
        });
    }

    private void updateButtonToVisitOfflinePage(Button button) {
        mOfflineButtonType = OfflineButtonType.VISIT;
        button.setText(getString(R.string.bookmark_btn_offline_page_visit));
        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                recordOfflineButtonAction(true);
                openBookmark();
            }
        });
    }

    private void openBookmark() {
        // TODO(kkimlabs): Refactor this out to handle the intent in ChromeActivity.
        // If this activity was started via startActivityForResult(), set the result. Otherwise,
        // launch the bookmark directly.
        if (getCallingActivity() != null) {
            Intent intent = new Intent();
            intent.putExtra(
                    EnhancedBookmarkActivity.INTENT_VISIT_BOOKMARK_ID, mBookmarkId.toString());
            setResult(RESULT_OK, intent);
        } else {
            EnhancedBookmarkUtils.openBookmark(
                    mModel, this, mBookmarkId, LaunchLocation.BOOKMARK_EDITOR);
        }
        finish();
    }

    private void recordOfflineButtonAction(boolean clicked) {
        // If button type is not set, it means that either offline section is not shown or we have
        // already recorded the click action.
        if (mOfflineButtonType == OfflineButtonType.NONE) {
            return;
        }

        assert mOfflineButtonType == OfflineButtonType.SAVE
                || mOfflineButtonType == OfflineButtonType.REMOVE
                || mOfflineButtonType == OfflineButtonType.VISIT;

        if (clicked) {
            if (mOfflineButtonType == OfflineButtonType.SAVE) {
                RecordUserAction.record("OfflinePages.Edit.SaveButtonClicked");
            } else if (mOfflineButtonType == OfflineButtonType.REMOVE) {
                RecordUserAction.record("OfflinePages.Edit.RemoveButtonClicked");
            } else if (mOfflineButtonType == OfflineButtonType.VISIT) {
                RecordUserAction.record("OfflinePages.Edit.VisitButtonClicked");
            }
        } else {
            if (mOfflineButtonType == OfflineButtonType.SAVE) {
                RecordUserAction.record("OfflinePages.Edit.SaveButtonNotClicked");
            } else if (mOfflineButtonType == OfflineButtonType.REMOVE) {
                RecordUserAction.record("OfflinePages.Edit.RemoveButtonNotClicked");
            } else if (mOfflineButtonType == OfflineButtonType.VISIT) {
                RecordUserAction.record("OfflinePages.Edit.VisitButtonNotClicked");
            }
        }

        mOfflineButtonType = OfflineButtonType.NONE;
    }
}

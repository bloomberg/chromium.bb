// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enhancedbookmarks;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.FrameLayout;
import android.widget.ListPopupWindow;
import android.widget.TextView;

import com.google.android.apps.chrome.R;

import org.chromium.chrome.browser.BookmarksBridge.BookmarkItem;
import org.chromium.chrome.browser.enhanced_bookmarks.LaunchLocation;
import org.chromium.chrome.browser.enhancedbookmarks.EnhancedBookmarkItemsAdapter.BookmarkGrid;
import org.chromium.chrome.browser.enhancedbookmarks.EnhancedBookmarkManager.UIState;
import org.chromium.chrome.browser.enhancedbookmarks.EnhancedBookmarkSalientImageView.SalientImageDrawableFactory;
import org.chromium.chrome.browser.widget.CustomShapeDrawable.CircularDrawable;
import org.chromium.chrome.browser.widget.CustomShapeDrawable.TopRoundedCornerDrawable;
import org.chromium.chrome.browser.widget.TintedImageButton;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;

import java.util.List;

/**
 * A view that shows a bookmark's title, screenshot, URL, etc, shown in the enhanced bookmarks UI.
 */
abstract class EnhancedBookmarkItem extends FrameLayout implements EnhancedBookmarkUIObserver,
        SalientImageDrawableFactory, BookmarkGrid {
    /**
     * The item to show in list view mode.
     */
    @SuppressLint("Instantiatable")
    static class EnhancedBookmarkListItem extends EnhancedBookmarkItem {
        public EnhancedBookmarkListItem(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        @Override
        public Drawable getSalientDrawable(int color) {
            return new CircularDrawable(color);
        }

        @Override
        public Drawable getSalientDrawable(Bitmap bitmap) {
            return new CircularDrawable(bitmap);
        }
    }

    /**
     * The item to show in grid mode.
     */
    @SuppressLint("Instantiatable")
    static class EnhancedBookmarkGridItem extends EnhancedBookmarkItem {
        public EnhancedBookmarkGridItem(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        @Override
        public Drawable getSalientDrawable(int color) {
            return new TopRoundedCornerDrawable(color, getResources().getDimensionPixelSize(
                    R.dimen.enhanced_bookmark_item_corner_radius));
        }

        @Override
        public Drawable getSalientDrawable(Bitmap bitmap) {
            return new TopRoundedCornerDrawable(bitmap, getResources().getDimensionPixelSize(
                    R.dimen.enhanced_bookmark_item_corner_radius));
        }
    }

    private EnhancedBookmarkSalientImageView mSalientImageView;
    private TintedImageButton mMoreIcon;
    private TextView mFolderTitleView;
    private TextView mTitleView;
    private TextView mUrlView;
    private EnhancedBookmarkItemHighlightView mHighlightView;

    private BookmarkId mBookmarkId;
    private EnhancedBookmarkDelegate mDelegate;
    private ListPopupWindow mPopupMenu;
    private boolean mIsAttachedToWindow = false;

    /**
     * Constructor for inflating from XML.
     */
    public EnhancedBookmarkItem(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mSalientImageView = (EnhancedBookmarkSalientImageView) findViewById(R.id.bookamrk_image);
        mMoreIcon = (TintedImageButton) findViewById(R.id.more);
        mMoreIcon.setOnClickListener(this);
        mMoreIcon.setColorFilterMode(PorterDuff.Mode.SRC.MULTIPLY);
        mFolderTitleView = (TextView) findViewById(R.id.folder_title);
        mTitleView = (TextView) findViewById(R.id.title);
        mUrlView = (TextView) findViewById(R.id.url);
        mHighlightView = (EnhancedBookmarkItemHighlightView) findViewById(R.id.highlight);
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // Ensure all items are of the same width.
        int updatedWidthMeasureSpec = MeasureSpec.makeMeasureSpec(
                MeasureSpec.getSize(widthMeasureSpec),
                MeasureSpec.EXACTLY);
        super.onMeasure(updatedWidthMeasureSpec, heightMeasureSpec);
    }

    @Override
    public void setBookmarkId(BookmarkId bookmarkId) {
        mBookmarkId = bookmarkId;
        updateBookmarkInfo();
    }

    @Override
    public BookmarkId getBookmarkId() {
        return mBookmarkId;
    }

    /**
     * Show drop-down menu after user click on more-info icon
     * @param view The anchor view for the menu
     */
    private void showMenu(View view) {
        if (mPopupMenu == null) {
            mPopupMenu = new ListPopupWindow(getContext(), null, 0,
                    R.style.EnhancedBookmarkMenuStyle);
            mPopupMenu.setAdapter(new ArrayAdapter<String>(
                    getContext(), R.layout.eb_popup_item, new String[] {
                            getContext().getString(R.string.enhanced_bookmark_item_select),
                            getContext().getString(R.string.enhanced_bookmark_item_edit),
                            getContext().getString(R.string.enhanced_bookmark_item_move),
                            getContext().getString(R.string.enhanced_bookmark_item_delete)}) {
                private static final int MOVE_POSITION = 1;

                @Override
                public boolean areAllItemsEnabled() {
                    return false;
                }

                @Override
                public boolean isEnabled(int position) {
                    // Partner bookmarks can't move, so disable that.
                    return mBookmarkId.getType() != BookmarkType.PARTNER
                            || position != MOVE_POSITION;
                }

                @Override
                public View getView(int position, View convertView, ViewGroup parent) {
                    View view = super.getView(position, convertView, parent);
                    view.setEnabled(isEnabled(position));
                    return view;
                }
            });
            mPopupMenu.setAnchorView(view);
            mPopupMenu.setWidth(getResources().getDimensionPixelSize(
                            R.dimen.enhanced_bookmark_item_popup_width));
            mPopupMenu.setVerticalOffset(-view.getHeight());
            mPopupMenu.setModal(true);
            mPopupMenu.setOnItemClickListener(new OnItemClickListener() {
                @Override
                public void onItemClick(AdapterView<?> parent, View view, int position,
                        long id) {
                    if (position == 0) {
                        setChecked(mDelegate.toggleSelectionForBookmark(mBookmarkId));
                    } else if (position == 1) {
                        mDelegate.startDetailActivity(mBookmarkId, mSalientImageView);
                    } else if (position == 2) {
                        EnhancedBookmarkFolderSelectActivity.startFolderSelectActivity(getContext(),
                                mBookmarkId);
                    } else if (position == 3) {
                        mDelegate.getModel().deleteBookmarks(mBookmarkId);
                    }
                    mPopupMenu.dismiss();
                }
            });
        }
        mPopupMenu.show();
        mPopupMenu.getListView().setDivider(null);
    }

    private void initialize() {
        mDelegate.addUIObserver(this);
        updateSelectionState();
    }

    private void cleanup() {
        if (mPopupMenu != null) mPopupMenu.dismiss();
        if (mDelegate != null) mDelegate.removeUIObserver(this);
    }

    private void updateSelectionState() {
        mMoreIcon.setClickable(!mDelegate.isSelectionEnabled());
    }

    /**
     * Update the UI to reflect the current bookmark information.
     */
    void updateBookmarkInfo() {
        if (mBookmarkId == null) return;

        BookmarkItem bookmarkItem = mDelegate.getModel().getBookmarkById(mBookmarkId);
        mMoreIcon.setVisibility(bookmarkItem.isEditable() ? VISIBLE : GONE);

        mTitleView.setText(bookmarkItem.getTitle());
        mUrlView.setText(Uri.parse(bookmarkItem.getUrl()).getHost());

        mFolderTitleView.setVisibility(View.INVISIBLE);
        BookmarkId parentId = bookmarkItem.getParentId();
        // On folder mode, folder name is shown at top so no need to show it again.
        if (mDelegate.getCurrentState() != UIState.STATE_FOLDER
                && parentId != null) {
            BookmarkItem parentItem = mDelegate.getModel().getBookmarkById(parentId);
            if (parentItem != null) {
                mFolderTitleView.setVisibility(View.VISIBLE);
                mFolderTitleView.setText(parentItem.getTitle());
            }
        }

        mSalientImageView.load(mDelegate.getModel(), bookmarkItem.getUrl(),
                EnhancedBookmarkUtils.generateBackgroundColor(bookmarkItem), this);
    }

    @Override
    public void onClick(View view) {
        if (view == mMoreIcon) {
            showMenu(view);
        } else {
            int launchLocation = -1;
            switch (mDelegate.getCurrentState()) {
                case UIState.STATE_ALL_BOOKMARKS:
                    launchLocation = LaunchLocation.ALL_ITEMS;
                    break;
                case UIState.STATE_FOLDER:
                    launchLocation = LaunchLocation.FOLDER;
                    break;
                case UIState.STATE_LOADING:
                    assert false :
                            "The main content shouldn't be inflated if it's still loading";
                    break;
                default:
                    assert false : "State not valid";
                    break;
            }
            mDelegate.openBookmark(mBookmarkId, launchLocation);
        }
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        mIsAttachedToWindow = true;
        if (mDelegate != null) initialize();
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mIsAttachedToWindow = false;
        cleanup();
    }

    // Checkable implementations.

    @Override
    public boolean isChecked() {
        return mHighlightView.isChecked();
    }

    @Override
    public void toggle() {
        setChecked(!isChecked());
    }

    @Override
    public void setChecked(boolean checked) {
        mHighlightView.setChecked(checked);
    }

    // EnhancedBookmarkUIObserver implementations.

    @Override
    public void onEnhancedBookmarkDelegateInitialized(EnhancedBookmarkDelegate delegate) {
        assert mDelegate == null;
        mDelegate = delegate;
        if (mIsAttachedToWindow) initialize();
    }

    @Override
    public void onDestroy() {
        cleanup();
    }

    @Override
    public void onAllBookmarksStateSet() {
    }

    @Override
    public void onFolderStateSet(BookmarkId folder) {
    }

    @Override
    public void onSelectionStateChange(List<BookmarkId> selectedBookmarks) {
        updateSelectionState();
    }

    @Override
    public void onListModeChange(boolean isListModeEnabled) {}
}

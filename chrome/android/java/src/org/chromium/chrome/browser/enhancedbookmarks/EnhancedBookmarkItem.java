// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enhancedbookmarks;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.PorterDuff;
import android.graphics.drawable.BitmapDrawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawableFactory;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.ListPopupWindow;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.BookmarksBridge.BookmarkItem;
import org.chromium.chrome.browser.enhanced_bookmarks.LaunchLocation;
import org.chromium.chrome.browser.enhancedbookmarks.EnhancedBookmarkItemsAdapter.BookmarkGrid;
import org.chromium.chrome.browser.enhancedbookmarks.EnhancedBookmarkManager.UIState;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.chrome.browser.widget.TintedImageButton;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;

import java.util.List;

/**
 * A view that shows a bookmark's title, screenshot, URL, etc, shown in the enhanced bookmarks UI.
 */
public class EnhancedBookmarkItem extends FrameLayout implements EnhancedBookmarkUIObserver,
        BookmarkGrid, LargeIconCallback {

    private ImageView mIconImageView;
    private TintedImageButton mMoreIcon;
    private TextView mTitleView;
    private EnhancedBookmarkItemHighlightView mHighlightView;
    private String mUrl;
    private RoundedIconGenerator mIconGenerator;
    private LargeIconBridge mLargeIconBridge;
    private int mMinIconSize;
    private int mDisplayedIconSize;
    private int mCornerRadius;

    private BookmarkId mBookmarkId;
    private EnhancedBookmarkDelegate mDelegate;
    private ListPopupWindow mPopupMenu;
    private boolean mIsAttachedToWindow = false;

    /**
     * Constructor for inflating from XML.
     */
    public EnhancedBookmarkItem(Context context, AttributeSet attrs) {
        super(context, attrs);
        mCornerRadius = getResources().getDimensionPixelSize(
                R.dimen.enhanced_bookmark_item_corner_radius);
        mMinIconSize = (int) getResources().getDimension(
                R.dimen.enhanced_bookmark_item_min_icon_size);
        mDisplayedIconSize = getResources().getDimensionPixelSize(
                R.dimen.enhanced_bookmark_item_icon_size);
        int textSize = getResources().getDimensionPixelSize(
                R.dimen.enhnaced_bookmark_item_icon_text_size);
        int iconColor = getResources().getColor(R.color.enhanced_bookmark_icon_background_color);
        mIconGenerator = new RoundedIconGenerator(mDisplayedIconSize , mDisplayedIconSize,
                mCornerRadius, iconColor, textSize);
        mLargeIconBridge = new LargeIconBridge();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mIconImageView = (ImageView) findViewById(R.id.bookmark_image);
        mMoreIcon = (TintedImageButton) findViewById(R.id.more);
        mMoreIcon.setOnClickListener(this);
        mMoreIcon.setColorFilterMode(PorterDuff.Mode.SRC.MULTIPLY);
        mTitleView = (TextView) findViewById(R.id.title);
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
                        EnhancedBookmarkUtils.startEditActivity(getContext(), mBookmarkId);
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
        mUrl = bookmarkItem.getUrl();
        mIconImageView.setImageDrawable(null);
        mLargeIconBridge.getLargeIconForUrl(Profile.getLastUsedProfile(), mUrl, mMinIconSize, this);
        mTitleView.setText(bookmarkItem.getTitle());
        mMoreIcon.setVisibility(bookmarkItem.isEditable() ? VISIBLE : GONE);
    }

    @Override
    public void onLargeIconAvailable(Bitmap icon, int fallbackColor) {
        if (icon == null) {
            mIconGenerator.setBackgroundColor(fallbackColor);
            icon = mIconGenerator.generateIconForUrl(mUrl);
            mIconImageView.setImageDrawable(new BitmapDrawable(getResources(), icon));
        } else {
            RoundedBitmapDrawable roundedIcon = RoundedBitmapDrawableFactory.create(
                    getResources(),
                    Bitmap.createScaledBitmap(icon, mDisplayedIconSize, mDisplayedIconSize, true));
            roundedIcon.setCornerRadius(mCornerRadius);
            mIconImageView.setImageDrawable(roundedIcon);
        }
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
}

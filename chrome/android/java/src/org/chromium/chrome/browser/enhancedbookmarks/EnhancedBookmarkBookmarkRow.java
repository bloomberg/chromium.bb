// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enhancedbookmarks;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawableFactory;
import android.text.format.Formatter;
import android.util.AttributeSet;
import android.view.View;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmark.BookmarksBridge.BookmarkItem;
import org.chromium.chrome.browser.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.OfflinePageItem;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.components.bookmarks.BookmarkId;

/**
 * A row view that shows bookmark info in the enhanced bookmarks UI.
 */
public class EnhancedBookmarkBookmarkRow extends EnhancedBookmarkRow implements LargeIconCallback {

    private String mUrl;
    private RoundedIconGenerator mIconGenerator;
    private final int mMinIconSize;
    private final int mDisplayedIconSize;
    private final int mCornerRadius;

    /**
     * Constructor for inflating from XML.
     */
    public EnhancedBookmarkBookmarkRow(Context context, AttributeSet attrs) {
        super(context, attrs);
        mCornerRadius = getResources().getDimensionPixelSize(
                R.dimen.enhanced_bookmark_item_corner_radius);
        mMinIconSize = (int) getResources().getDimension(
                R.dimen.enhanced_bookmark_item_min_icon_size);
        mDisplayedIconSize = getResources().getDimensionPixelSize(
                R.dimen.enhanced_bookmark_item_icon_size);
        int textSize = getResources().getDimensionPixelSize(
                R.dimen.enhanced_bookmark_item_icon_text_size);
        int iconColor = ApiCompatibilityUtils.getColor(getResources(),
                R.color.enhanced_bookmark_icon_background_color);
        mIconGenerator = new RoundedIconGenerator(mDisplayedIconSize , mDisplayedIconSize,
                mCornerRadius, iconColor, textSize);
    }

    // EnhancedBookmarkRow implementation.

    @Override
    public void onClick() {
        int launchLocation = -1;
        switch (mDelegate.getCurrentState()) {
            case EnhancedBookmarkUIState.STATE_ALL_BOOKMARKS:
                launchLocation = BookmarkLaunchLocation.ALL_ITEMS;
                break;
            case EnhancedBookmarkUIState.STATE_FOLDER:
                launchLocation = BookmarkLaunchLocation.FOLDER;
                break;
            case EnhancedBookmarkUIState.STATE_FILTER:
                launchLocation = BookmarkLaunchLocation.FILTER;
                break;
            case EnhancedBookmarkUIState.STATE_LOADING:
                assert false :
                        "The main content shouldn't be inflated if it's still loading";
                break;
            default:
                assert false : "State not valid";
                break;
        }
        mDelegate.openBookmark(mBookmarkId, launchLocation);
    }

    @Override
    BookmarkItem setBookmarkId(BookmarkId bookmarkId) {
        BookmarkItem item = super.setBookmarkId(bookmarkId);
        mUrl = item.getUrl();
        mIconImageView.setImageDrawable(null);
        mTitleView.setText(item.getTitle());
        mDelegate.getLargeIconBridge().getLargeIconForUrl(mUrl, mMinIconSize, this);
        updateOfflinePageSize(bookmarkId);
        return item;
    }

    private void updateOfflinePageSize(BookmarkId bookmarkId) {
        OfflinePageItem offlinePage = null;
        OfflinePageBridge bridge = mDelegate.getModel().getOfflinePageBridge();
        if (mDelegate.getCurrentState() == EnhancedBookmarkUIState.STATE_FILTER && bridge != null) {
            offlinePage = bridge.getPageByBookmarkId(bookmarkId);
        }
        TextView textView = (TextView) findViewById(R.id.offline_page_size);
        View bookmarkRowView = findViewById(R.id.bookmark_row);
        if (offlinePage != null) {
            int verticalPadding = textView.getResources().getDimensionPixelSize(
                    R.dimen.offline_page_item_vertical_spacing);
            textView.setText(Formatter.formatFileSize(getContext(), offlinePage.getFileSize()));
            // Get the embedded bookmark_row layout, and add padding.  This is because the entries
            // in filter view are larger (contain more items) than normal bookmark view.
            bookmarkRowView.setPadding(0, verticalPadding / 2, 0, verticalPadding / 2);
            textView.setVisibility(View.VISIBLE);
        } else {
            textView.setVisibility(View.GONE);
            // Remove padding when we leave filter view.
            bookmarkRowView.setPadding(0, 0, 0, 0);
        }
    }

    // LargeIconCallback implementation.

    @Override
    public void onLargeIconAvailable(Bitmap icon, int fallbackColor) {
        if (icon == null) {
            mIconGenerator.setBackgroundColor(fallbackColor);
            icon = mIconGenerator.generateIconForUrl(mUrl);
            mIconImageView.setImageDrawable(new BitmapDrawable(getResources(), icon));
        } else {
            RoundedBitmapDrawable roundedIcon = RoundedBitmapDrawableFactory.create(
                    getResources(),
                    Bitmap.createScaledBitmap(icon, mDisplayedIconSize, mDisplayedIconSize, false));
            roundedIcon.setCornerRadius(mCornerRadius);
            mIconImageView.setImageDrawable(roundedIcon);
        }
    }
}

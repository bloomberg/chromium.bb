// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enhancedbookmarks;

import android.content.Context;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.view.View;
import android.widget.Checkable;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.components.bookmarks.BookmarkId;

import java.util.List;

/**
 * Container for all bookmark items shown in enhanced bookmark manager.
 */
public class EnhancedBookmarkRecyclerView extends RecyclerView implements
        EnhancedBookmarkUIObserver {
    private static final int MINIMUM_LARGE_TABLET_WIDTH_DP = 720;
    private static Boolean sIsLargeTablet = null;

    private EnhancedBookmarkDelegate mDelegate;
    private View mEmptyView;

    /**
     * @return True if the current device's minimum dimension is larger than 720dp.
     */
    static boolean isLargeTablet(Context context) {
        // TODO(ianwen): move this function to DeviceFormFactor.java in upstream.
        if (sIsLargeTablet == null) {
            int minimumScreenWidthDp = context.getResources().getConfiguration()
                    .smallestScreenWidthDp;
            sIsLargeTablet = minimumScreenWidthDp >= MINIMUM_LARGE_TABLET_WIDTH_DP;
        }
        return sIsLargeTablet;
    }

    /**
     * Constructs a new instance of enhanced bookmark recycler view.
     */
    public EnhancedBookmarkRecyclerView(Context context, AttributeSet attrs) {
        super(context, attrs);

        setLayoutManager(new LinearLayoutManager(context));
        setHasFixedSize(true);
    }

    /**
     * Sets the view to be shown if there are no items in adapter.
     */
    void setEmptyView(View emptyView) {
        mEmptyView = emptyView;
    }

    // RecyclerView implementation

    @Override
    public void setLayoutParams(android.view.ViewGroup.LayoutParams params) {
        if (isLargeTablet(getContext())) {
            int marginPx = getContext().getResources().getDimensionPixelSize(
                    R.dimen.enhanced_bookmark_list_mode_background_margin);
            assert params instanceof MarginLayoutParams;
            ((MarginLayoutParams) params).setMargins(marginPx, marginPx, marginPx, marginPx);
        }

        super.setLayoutParams(params);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        if (isLargeTablet(getContext())) {
            setBackgroundResource(R.drawable.eb_large_tablet_list_background);
        } else {
            setBackgroundColor(getContext().getResources().getColor(android.R.color.white));
        }
    }

    @Override
    public void setAdapter(final Adapter adapter) {
        super.setAdapter(adapter);
        adapter.registerAdapterDataObserver(new AdapterDataObserver() {
            @Override
            public void onChanged() {
                super.onChanged();
                updateEmptyViewVisibility(adapter);
            }

            @Override
            public void onItemRangeInserted(int positionStart, int itemCount) {
                super.onItemRangeInserted(positionStart, itemCount);
                updateEmptyViewVisibility(adapter);
            }

            @Override
            public void onItemRangeRemoved(int positionStart, int itemCount) {
                super.onItemRangeRemoved(positionStart, itemCount);
                updateEmptyViewVisibility(adapter);
            }
        });
        updateEmptyViewVisibility(adapter);
    }

    @VisibleForTesting
    @Override
    public EnhancedBookmarkItemsAdapter getAdapter() {
        return (EnhancedBookmarkItemsAdapter) super.getAdapter();
    }

    /**
     * Unlike ListView or GridView, RecyclerView does not provide default empty
     * view implementation. We need to check it ourselves.
     */
    private void updateEmptyViewVisibility(Adapter adapter) {
        mEmptyView.setVisibility(adapter.getItemCount() == 0 ? View.VISIBLE : View.GONE);
    }

    // EnhancedBookmarkUIObserver implementations

    @Override
    public void onEnhancedBookmarkDelegateInitialized(EnhancedBookmarkDelegate delegate) {
        mDelegate = delegate;
        mDelegate.addUIObserver(this);

        EnhancedBookmarkItemsAdapter adapter = new EnhancedBookmarkItemsAdapter(getContext());
        adapter.onEnhancedBookmarkDelegateInitialized(mDelegate);
        setAdapter(adapter);
    }

    @Override
    public void onDestroy() {
        mDelegate.removeUIObserver(this);
    }

    @Override
    public void onAllBookmarksStateSet() {
        scrollToPosition(0);
    }

    @Override
    public void onFolderStateSet(BookmarkId folder) {
        scrollToPosition(0);
    }

    @Override
    public void onSelectionStateChange(List<BookmarkId> selectedBookmarks) {
        if (!mDelegate.isSelectionEnabled()) {
            for (int i = 0; i < getLayoutManager().getChildCount(); ++i) {
                View child = getLayoutManager().getChildAt(i);
                if (child instanceof Checkable) ((Checkable) child).setChecked(false);
            }
        }
    }
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enhancedbookmarks;

import android.content.Context;
import android.graphics.Rect;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.GridLayoutManager.SpanSizeLookup;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.view.View;
import android.widget.Checkable;
import android.widget.RelativeLayout;

import com.google.android.apps.chrome.R;

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
    private int mGridMinWidthPx;
    private int mItemHalfSpacePx;
    private ItemDecoration mItemDecoration;

    /**
     * @return True if the current device's minimum dimension is larger than 720dp.
     */
    public static boolean isLargeTablet(Context context) {
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
        // Span count will be dynamically computed in onMeasure().
        setLayoutManager(new GridLayoutManager(context, 1));
        mGridMinWidthPx = getResources().getDimensionPixelSize(
                R.dimen.enhanced_bookmark_min_grid_width);
        mItemHalfSpacePx = getResources().getDimensionPixelSize(
                R.dimen.enhanced_bookmark_item_half_space);
        mItemDecoration = new ItemDecoration() {
            @Override
            public void getItemOffsets(Rect outRect, View view, RecyclerView parent, State state) {
                outRect.set(mItemHalfSpacePx, mItemHalfSpacePx, mItemHalfSpacePx, mItemHalfSpacePx);
            }
        };
        setHasFixedSize(true);
    }

    @Override
    protected void onMeasure(int widthSpec, int heightSpec) {
        super.onMeasure(widthSpec, heightSpec);
        int width = MeasureSpec.getSize(widthSpec);
        if (width != 0 && mDelegate != null && !mDelegate.isListModeEnabled()) {
            int spans = width / mGridMinWidthPx;
            if (spans > 0) {
                getLayoutManager().setSpanCount(spans);
                if (getAdapter() != null) getAdapter().setNumColumns(spans);
            }
        }
    }

    /**
     * Sets the view to be shown if there are no items in adapter.
     */
    public void setEmptyView(View emptyView) {
        mEmptyView = emptyView;
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

    @Override
    public EnhancedBookmarkItemsAdapter getAdapter() {
        return (EnhancedBookmarkItemsAdapter) super.getAdapter();
    }

    @Override
    public GridLayoutManager getLayoutManager() {
        return (GridLayoutManager) super.getLayoutManager();
    }

    /**
     * Unlike ListView or GridView, RecyclerView does not provide default empty
     * view implementation. We need to check it ourselves.
     */
    private void updateEmptyViewVisibility(Adapter adapter) {
        mEmptyView.setVisibility(adapter.getItemCount() == 0 ? View.VISIBLE : View.GONE);
    }

    private void setMargin(int marginInPx) {
        RelativeLayout.LayoutParams lp = (RelativeLayout.LayoutParams) getLayoutParams();
        lp.setMargins(marginInPx, marginInPx, marginInPx, marginInPx);
        setLayoutParams(lp);
    }

    // EnhancedBookmarkUIObserver implementations

    @Override
    public void onEnhancedBookmarkDelegateInitialized(EnhancedBookmarkDelegate delegate) {
        mDelegate = delegate;
        mDelegate.addUIObserver(this);

        EnhancedBookmarkItemsAdapter adapter = new EnhancedBookmarkItemsAdapter(getContext());
        adapter.onEnhancedBookmarkDelegateInitialized(mDelegate);
        setAdapter(adapter);

        getLayoutManager().setSpanSizeLookup(new SpanSizeLookup() {
            @Override
            public int getSpanSize(int position) {
                return getAdapter().isHeader(position) ? getLayoutManager().getSpanCount() : 1;
            }
        });
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

    @Override
    public void onListModeChange(final boolean isListModeEnabled) {
        if (isListModeEnabled) {
            removeItemDecoration(mItemDecoration);
            setPadding(0, 0, 0, 0);
            getLayoutManager().setSpanCount(1);

            if (isLargeTablet(getContext())) {
                setClipToPadding(true);
                setBackgroundResource(R.drawable.eb_item_tile);
                setMargin(getContext().getResources().getDimensionPixelSize(
                        R.dimen.enhanced_bookmark_list_mode_background_margin));
            } else {
                setBackgroundColor(getContext().getResources().getColor(android.R.color.white));
            }
        } else {
            addItemDecoration(mItemDecoration);
            setPadding(mItemHalfSpacePx, mItemHalfSpacePx, mItemHalfSpacePx, mItemHalfSpacePx);

            if (isLargeTablet(getContext())) {
                setClipToPadding(false);
                setBackgroundResource(0);
                setMargin(0);
            } else {
                setBackgroundColor(
                        getContext().getResources().getColor(R.color.default_primary_color));
            }
            requestLayout();
        }

        // TODO(ianwen): remember scroll position and scroll back after mode switching.
        mDelegate.notifyStateChange(this);
    }
}

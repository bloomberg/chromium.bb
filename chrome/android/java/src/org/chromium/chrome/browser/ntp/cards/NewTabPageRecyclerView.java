// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.content.Context;
import android.content.res.Resources;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import org.chromium.chrome.R;

/**
 * Simple wrapper on top of a RecyclerView that will acquire focus when tapped.  Ensures the
 * New Tab page receives focus when clicked.
 */
public class NewTabPageRecyclerView extends RecyclerView {
    /**
     * Minimum height of the bottom spacing item.
     */
    private static final int MIN_BOTTOM_SPACING = 0;

    /**
     * Position in the adapter of the item we snap the scroll at, when switching between above and
     * below the fold.
     */
    private static final int SNAP_ITEM_ADAPTER_POSITION = 1;

    private final GestureDetector mGestureDetector;
    private final LinearLayoutManager mLayoutManager;
    private final int mToolbarHeight;

    /**
     * Total height of the items being dismissed.  Tracked to allow the bottom space to compensate
     * for their removal animation and avoid moving the scroll position.
     */
    private int mCompensationHeight;

    /**
     * Constructor needed to inflate from XML.
     */
    public NewTabPageRecyclerView(Context context, AttributeSet attrs) {
        super(context, attrs);

        mGestureDetector =
                new GestureDetector(getContext(), new GestureDetector.SimpleOnGestureListener() {
                    @Override
                    public boolean onSingleTapUp(MotionEvent e) {
                        boolean retVal = super.onSingleTapUp(e);
                        requestFocus();
                        return retVal;
                    }
                });
        mLayoutManager = new LinearLayoutManager(getContext());
        setLayoutManager(mLayoutManager);

        Resources res = context.getResources();
        mToolbarHeight = res.getDimensionPixelSize(R.dimen.toolbar_height_no_shadow)
                + res.getDimensionPixelSize(R.dimen.toolbar_progress_bar_height);
    }

    public boolean isFirstItemVisible() {
        return mLayoutManager.findFirstVisibleItemPosition() == 0;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        mGestureDetector.onTouchEvent(ev);
        return super.onInterceptTouchEvent(ev);
    }

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        // Action down would already have been handled in onInterceptTouchEvent
        if (ev.getActionMasked() != MotionEvent.ACTION_DOWN) {
            mGestureDetector.onTouchEvent(ev);
        }
        return super.onTouchEvent(ev);
    }

    @Override
    public void focusableViewAvailable(View v) {
        // To avoid odd jumps during NTP animation transitions, we do not attempt to give focus
        // to child views if this scroll view already has focus.
        if (hasFocus()) return;
        super.focusableViewAvailable(v);
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        // Fixes landscape transitions when unfocusing the URL bar: crbug.com/288546
        outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_FULLSCREEN;
        return super.onCreateInputConnection(outAttrs);
    }

    public LinearLayoutManager getLinearLayoutManager() {
        return mLayoutManager;
    }

    /**
     * Updates the space added at the end of the list to make sure the above/below the fold
     * distinction can be preserved.
     */
    public void refreshBottomSpacing() {
        ViewHolder bottomSpacingViewHolder =
                findViewHolderForAdapterPosition(getAdapter().getItemCount() - 1);

        // It might not be in the layout yet if it's not visible or ready to be displayed.
        if (bottomSpacingViewHolder == null) return;

        assert bottomSpacingViewHolder.getItemViewType() == NewTabPageListItem.VIEW_TYPE_SPACING;
        bottomSpacingViewHolder.itemView.requestLayout();
    }

    /**
     * Calculates the height of the bottom spacing item, such that there is always enough content
     * below the fold to push the header up to to the top of the screen.
     */
    int calculateBottomSpacing() {
        int firstVisiblePos = mLayoutManager.findFirstVisibleItemPosition();

        // We have enough items to fill the view, since the snap point item is not even visible.
        if (firstVisiblePos > SNAP_ITEM_ADAPTER_POSITION) return MIN_BOTTOM_SPACING;

        // The spacing item is the last item, the last content item is directly above that.
        int lastContentItemPosition = getAdapter().getItemCount() - 2;
        int contentHeight =
                findViewHolderForAdapterPosition(lastContentItemPosition).itemView.getBottom()
                - findViewHolderForAdapterPosition(SNAP_ITEM_ADAPTER_POSITION).itemView.getTop();
        int bottomSpacing = getHeight() - mToolbarHeight - contentHeight + mCompensationHeight;

        return Math.max(MIN_BOTTOM_SPACING, bottomSpacing);
    }

    /** Called when an item is in the process of being removed from the view. */
    void onItemDismissStarted(View itemView) {
        mCompensationHeight += itemView.getHeight();
    }

    /** Called when an item has finished being removed from the view. */
    void onItemDismissFinished(View itemView) {
        mCompensationHeight -= itemView.getHeight();
        assert mCompensationHeight >= 0;
    }
}

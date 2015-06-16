// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enhancedbookmarks;

import android.animation.ObjectAnimator;
import android.content.Context;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewTreeObserver.OnScrollChangedListener;
import android.widget.RelativeLayout;

import com.google.android.apps.chrome.R;

import org.chromium.chrome.browser.widget.FadingShadow;
import org.chromium.chrome.browser.widget.FadingShadowView;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.ui.interpolators.BakedBezierInterpolator;

import java.util.List;

/**
 * A ViewGroup that holds an {@link EnhancedBookmarkActionBar}, a {@link FadingShadowView}, a
 * {@link EnhancedBookmarkRecyclerView} and a {@link EnhancedBookmarkLoadingView}. On large
 * tablet, it can be replaced by a {@link EnhancedBookmarkSearchView} with the same size, located at
 * the right of the window.
 */
public class EnhancedBookmarkContentView extends RelativeLayout implements
        EnhancedBookmarkUIObserver {
    private static final int SHADOW_ANIMATION_DURATION_MS = 500;
    private EnhancedBookmarkDelegate mDelegate;
    private EnhancedBookmarkRecyclerView mItemsContainer;
    private EnhancedBookmarkActionBar mActionBar;
    private FadingShadowView mShadow;
    private EnhancedBookmarkLoadingView mLoadingView;
    private ObjectAnimator mShadowInAnim;

    private OnScrollChangedListener mScrollListener = new OnScrollChangedListener() {
        @Override
        public void onScrollChanged() {
            if (mShadow == null || mDelegate == null || mDelegate.isSelectionEnabled()
                    || mDelegate.isListModeEnabled()) return;
            int firstPosition = mItemsContainer.getLayoutManager()
                    .findFirstCompletelyVisibleItemPosition();
            if (firstPosition == 0 || firstPosition == RecyclerView.NO_POSITION) {
                if (mShadowInAnim.isStarted()) mShadowInAnim.cancel();
                mShadow.setStrength(0);
            } else {
                if (!mShadowInAnim.isStarted() && mShadow.getStrength() < 1.0f) {
                    mShadowInAnim.start();
                }
            }
        }
    };

    /**
     * Creates an instance of {@link EnhancedBookmarkContentView}. This constructor should be used
     * by the framework when inflating from XML.
     */
    public EnhancedBookmarkContentView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mItemsContainer = (EnhancedBookmarkRecyclerView) findViewById(
                R.id.eb_items_container);
        mItemsContainer.setEmptyView(findViewById(R.id.eb_empty_view));
        mItemsContainer.getViewTreeObserver().addOnScrollChangedListener(mScrollListener);
        mActionBar = (EnhancedBookmarkActionBar) findViewById(R.id.eb_action_bar);
        mLoadingView = (EnhancedBookmarkLoadingView) findViewById(R.id.eb_initial_loading_view);
        mShadow = (FadingShadowView) findViewById(R.id.shadow);
        mShadow.init(getResources().getColor(R.color.enhanced_bookmark_app_bar_shadow_color),
                FadingShadow.POSITION_TOP);
        mShadowInAnim = ObjectAnimator.ofFloat(mShadow, "Strength", 1.0f)
                .setDuration(SHADOW_ANIMATION_DURATION_MS);
        mShadowInAnim.setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE);
    }

    /**
     * Handles the event when user clicks back button and the UI is in selection mode.
     * @return True if there are selected bookmarks, and the back button is processed by this
     *         method. False otherwise.
     */
    public boolean onBackPressed() {
        if (mDelegate != null && mDelegate.isSelectionEnabled()) {
            mDelegate.clearSelection();
            return true;
        }
        return false;
    }

    void showLoadingUi() {
        mActionBar.showLoadingUi();
        mLoadingView.showLoadingUI();
    }

    // EnhancedBookmarkDelegate implementations.

    @Override
    public void onEnhancedBookmarkDelegateInitialized(EnhancedBookmarkDelegate delegate) {
        mDelegate = delegate;
        mDelegate.addUIObserver(this);
        mItemsContainer.onEnhancedBookmarkDelegateInitialized(mDelegate);
        mActionBar.onEnhancedBookmarkDelegateInitialized(mDelegate);
    }

    @Override
    public void onAllBookmarksStateSet() {
        mLoadingView.hideLoadingUI();
    }

    @Override
    public void onFolderStateSet(BookmarkId folder) {
        mLoadingView.hideLoadingUI();
    }

    @Override
    public void onSelectionStateChange(List<BookmarkId> selectedBookmarks) {
        if (!selectedBookmarks.isEmpty()) mShadow.setStrength(1.0f);
        else mScrollListener.onScrollChanged();
    }

    @Override
    public void onDestroy() {
        mDelegate.removeUIObserver(this);
    }

    @Override
    public void onListModeChange(boolean isListModeEnabled) {
        if (EnhancedBookmarkRecyclerView.isLargeTablet(getContext())) {
            if (isListModeEnabled) mShadow.setVisibility(View.GONE);
            else mShadow.setVisibility(View.VISIBLE);
        }
        mShadow.setStrength(mDelegate.isListModeEnabled() ? 1.0f : 0.0f);
    }
}

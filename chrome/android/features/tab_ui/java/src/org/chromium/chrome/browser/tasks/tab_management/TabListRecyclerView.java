// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Rect;
import android.os.SystemClock;
import android.support.annotation.Nullable;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewParent;

import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.ui.interpolators.BakedBezierInterpolator;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

/**
 * A custom RecyclerView implementation for the tab grid, to handle show/hide logic in class.
 */
class TabListRecyclerView extends RecyclerView {
    public static final long BASE_ANIMATION_DURATION_MS = 218;

    /**
     * Field trial parameter for downsampling scaling factor.
     */
    private static final String DOWNSAMPLING_SCALE_PARAM = "downsampling-scale";

    private static final float DEFAULT_DOWNSAMPLING_SCALE = 0.5f;

    /**
     * An interface to listen to visibility related changes on this {@link RecyclerView}.
     */
    interface VisibilityListener {
        /**
         * Called before the animation to show the tab list has started.
         * @param isAnimating Whether visibility is changing with animation
         */
        void startedShowing(boolean isAnimating);

        /**
         * Called when the animation to show the tab list is finished.
         */
        void finishedShowing();

        /**
         * Called before the animation to hide the tab list has started.
         * @param isAnimating Whether visibility is changing with animation
         */
        void startedHiding(boolean isAnimating);

        /**
         * Called when the animation to show the tab list is finished.
         */
        void finishedHiding();
    }

    private ValueAnimator mFadeInAnimator;
    private ValueAnimator mFadeOutAnimator;
    private VisibilityListener mListener;
    private ViewResourceAdapter mDynamicView;
    private long mLastDirtyTime;
    private long mOriginalAddDuration;

    /**
     * Basic constructor to use during inflation from xml.
     */
    public TabListRecyclerView(Context context, AttributeSet attributeSet) {
        super(context, attributeSet);
    }

    /**
     * Set the {@link VisibilityListener} that will listen on granular visibility events.
     * @param listener The {@link VisibilityListener} to use.
     */
    void setVisibilityListener(VisibilityListener listener) {
        mListener = listener;
    }

    void prepareOverview() {
        endAllAnimations();

        // Make all the items show up immediately.
        mOriginalAddDuration = getItemAnimator().getAddDuration();
        getItemAnimator().setAddDuration(0);
    }

    /**
     * Start showing the tab list.
     * @param animate Whether the visibility change should be animated.
     */
    void startShowing(boolean animate) {
        assert mFadeOutAnimator == null;
        mListener.startedShowing(animate);

        setAlpha(0);
        setVisibility(View.VISIBLE);
        mFadeInAnimator = ObjectAnimator.ofFloat(this, View.ALPHA, 1);
        mFadeInAnimator.setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE);
        mFadeInAnimator.setDuration(BASE_ANIMATION_DURATION_MS);
        mFadeInAnimator.start();
        mFadeInAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mFadeInAnimator = null;
                mListener.finishedShowing();
                // Restore the original value.
                getItemAnimator().setAddDuration(mOriginalAddDuration);

                if (mDynamicView != null)
                    mDynamicView.dropCachedBitmap();
            }
        });
        if (!animate) mFadeInAnimator.end();
    }

    /**
     * @return The ID for registering and using the dynamic resource in compositor.
     */
    int getResourceId() {
        return getId();
    }

    long getLastDirtyTimeForTesting() {
        return mLastDirtyTime;
    }

    private float getDownsamplingScale() {
        String scale = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.TAB_TO_GTS_ANIMATION, DOWNSAMPLING_SCALE_PARAM);
        try {
            return Float.valueOf(scale);
        } catch (NumberFormatException e) {
            return DEFAULT_DOWNSAMPLING_SCALE;
        }
    }

    /**
     * Create a DynamicResource for this RecyclerView.
     * The view resource can be obtained by {@link #getResourceId} in compositor layer.
     */
    void createDynamicView(DynamicResourceLoader loader) {
        mDynamicView = new ViewResourceAdapter(this) {
            @Override
            public boolean isDirty() {
                boolean dirty = super.isDirty();
                if (dirty) mLastDirtyTime = SystemClock.elapsedRealtime();
                return dirty;
            }
        };
        mDynamicView.setDownsamplingScale(getDownsamplingScale());
        loader.registerResource(getResourceId(), mDynamicView);
    }

    @SuppressLint("NewApi") // Used on O+, invalidateChildInParent used for previous versions.
    @Override
    public void onDescendantInvalidated(View child, View target) {
        super.onDescendantInvalidated(child, target);
        if (mDynamicView != null) {
            mDynamicView.invalidate(null);
        }
    }

    @Override
    public ViewParent invalidateChildInParent(int[] location, Rect dirty) {
        ViewParent retVal = super.invalidateChildInParent(location, dirty);
        if (mDynamicView != null) {
            mDynamicView.invalidate(dirty);
        }
        return retVal;
    }

    /**
     * Start hiding the tab list.
     * @param animate Whether the visibility change should be animated.
     */
    void startHiding(boolean animate) {
        endAllAnimations();
        mListener.startedHiding(animate);
        mFadeOutAnimator = ObjectAnimator.ofFloat(this, View.ALPHA, 0);
        mFadeOutAnimator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
        mFadeOutAnimator.setDuration(BASE_ANIMATION_DURATION_MS);
        mFadeOutAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mFadeOutAnimator = null;
                setVisibility(View.INVISIBLE);
                mListener.finishedHiding();
            }
        });
        mFadeOutAnimator.start();
        if (!animate) mFadeOutAnimator.end();
    }

    void postHiding() {
        if (mDynamicView != null) {
            mDynamicView.dropCachedBitmap();
        }
    }

    private void endAllAnimations() {
        if (mFadeInAnimator != null) {
            mFadeInAnimator.end();
        }
        if (mFadeOutAnimator != null) {
            mFadeOutAnimator.end();
        }
    }

    /**
     * @param currentTabIndex The the current tab's index in the model.
     * @return The {@link Rect} of the thumbnail of the current tab, relative to the
     *         {@link TabListRecyclerView} coordinates.
     */
    @Nullable
    Rect getRectOfCurrentThumbnail(int currentTabIndex) {
        TabGridViewHolder holder =
                (TabGridViewHolder) findViewHolderForAdapterPosition(currentTabIndex);
        if (holder == null) return null;

        int[] loc = new int[2];
        holder.thumbnail.getLocationInWindow(loc);
        Rect rect = new Rect(loc[0], loc[1], loc[0] + holder.thumbnail.getWidth(),
                loc[1] + holder.thumbnail.getHeight());
        getLocationInWindow(loc);
        rect.top -= loc[1];
        rect.bottom -= loc[1];
        return rect;
    }
}

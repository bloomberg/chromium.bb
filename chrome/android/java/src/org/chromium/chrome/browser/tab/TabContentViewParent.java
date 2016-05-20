// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Context;
import android.support.design.widget.CoordinatorLayout;
import android.support.design.widget.CoordinatorLayout.Behavior;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.banners.SwipableOverlayView;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Parent {@link FrameLayout} holding the infobar and content view of a tab.
 */
public class TabContentViewParent extends FrameLayout {
    // A wrapper is needed because infobar's translation is controlled by SwipableOverlayView.
    // Setting infobar's translation directly from this class will cause UI flickering.
    private final FrameLayout mInfobarWrapper;
    private final Behavior<?> mBehavior = new SnackbarAwareBehavior();

    public TabContentViewParent(Context context) {
        super(context);
        mInfobarWrapper = new FrameLayout(context);
        addView(mInfobarWrapper,
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
    }

    /**
     * Attach the infobar container to the view hierarchy.
     */
    public void addInfobarView(SwipableOverlayView infobarView, MarginLayoutParams lp) {
        mInfobarWrapper.addView(infobarView, lp);
    }

    /**
     * @return The {@link Behavior} that controls how children of this class animate together.
     */
    public Behavior<?> getBehavior() {
        return mBehavior;
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mInfobarWrapper.setTranslationY(0f);
    }

    private static class SnackbarAwareBehavior
            extends CoordinatorLayout.Behavior<TabContentViewParent> {
        @Override
        public boolean layoutDependsOn(CoordinatorLayout parent, TabContentViewParent child,
                View dependency) {
            // Disable coordination on tablet as they appear at different location on tablet.
            return dependency.getId() == R.id.snackbar
                    && !DeviceFormFactor.isTablet(child.getContext());
        }

        @Override
        public boolean onDependentViewChanged(CoordinatorLayout parent, TabContentViewParent child,
                View dependency) {
            child.mInfobarWrapper
                    .setTranslationY(dependency.getTranslationY() - dependency.getHeight());
            return true;
        }

        @Override
        public void onDependentViewRemoved(CoordinatorLayout parent, TabContentViewParent child,
                View dependency) {
            child.mInfobarWrapper.setTranslationY(0);
        }
    }
}

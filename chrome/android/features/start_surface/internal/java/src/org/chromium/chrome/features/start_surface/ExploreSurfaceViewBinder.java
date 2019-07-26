// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_EXPLORE_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_INCOGNITO;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;

import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.FrameLayout.LayoutParams;

import org.chromium.chrome.browser.feed.FeedSurfaceCoordinator;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binder for the explore surface. */
class ExploreSurfaceViewBinder {
    /** Interface to access {@link FeedSurfaceCoordinator} */
    interface FeedSurfaceDelegate {
        /**
         * Creates the {@link FeedSurfaceCoordinator} for the specified mode.
         * @param isIncognito Whether it is in incognito mode.
         * @return The {@link FeedSurfaceCoordinator}.
         */
        FeedSurfaceCoordinator createFeedSurfaceCoordinator(boolean isIncognito);
    }

    /** The class holds all views, their information and status. */
    public static class ViewHolder {
        public final ViewGroup containerView;
        public final int bottomControlsHeight;
        public final int topMargin;
        public final FeedSurfaceDelegate feedSurfaceDelegate;

        // feedSurfaceCoordinator will be rebuilt when incognito state is changed to use the right
        // profile and color, and it will be destroyed when overview mode is hiding to release
        // resources.
        public FeedSurfaceCoordinator feedSurfaceCoordinator;
        public boolean isIncognito;
        public boolean isShown;

        ViewHolder(ViewGroup containerView, int bottomControlsHeight, int topMargin,
                FeedSurfaceDelegate feedSurfaceDelegate) {
            this.containerView = containerView;
            this.bottomControlsHeight = bottomControlsHeight;
            this.topMargin = topMargin;
            this.feedSurfaceDelegate = feedSurfaceDelegate;
        }
    }

    public static void bind(PropertyModel model, ViewHolder view, PropertyKey propertyKey) {
        if (propertyKey == IS_INCOGNITO) {
            setIncognito(view, model.get(IS_INCOGNITO));
        } else if (propertyKey == IS_EXPLORE_SURFACE_VISIBLE) {
            setVisibility(view, model.get(IS_EXPLORE_SURFACE_VISIBLE));
        } else if (propertyKey == IS_SHOWING_OVERVIEW) {
            // Set the initial state if the explore surface is selected previously.
            setOverview(
                    view, model.get(IS_SHOWING_OVERVIEW), model.get(IS_EXPLORE_SURFACE_VISIBLE));
        }
    }

    private static void setIncognito(ViewHolder viewHolder, boolean isIncognito) {
        if (viewHolder.isIncognito == isIncognito) return;

        viewHolder.isIncognito = isIncognito;

        // Set invisible to remove the view from it's parent if it was showing and destroy the feed
        // surface coordinator (no matter it was shown or not).
        boolean wasShown = viewHolder.isShown;
        if (wasShown) setVisibility(viewHolder, false);
        destroy(viewHolder);

        // Set visible if it was shown, which will build the new feed surface coordinator according
        // to the new incognito state.
        if (wasShown) setVisibility(viewHolder, true);
    }

    /** This interface builds the feed surface coordinator when showing if needed. */
    private static void setVisibility(ViewHolder viewHolder, boolean shown) {
        if (viewHolder.isShown == shown) return;

        if (shown) {
            if (viewHolder.feedSurfaceCoordinator == null) {
                viewHolder.feedSurfaceCoordinator =
                        viewHolder.feedSurfaceDelegate.createFeedSurfaceCoordinator(
                                viewHolder.isIncognito);
            }
            FrameLayout.LayoutParams layoutParams = new FrameLayout.LayoutParams(
                    LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
            layoutParams.bottomMargin = viewHolder.bottomControlsHeight;
            layoutParams.topMargin = viewHolder.topMargin;
            viewHolder.containerView.addView(
                    viewHolder.feedSurfaceCoordinator.getView(), layoutParams);
        } else {
            if (viewHolder.feedSurfaceCoordinator != null)
                UiUtils.removeViewFromParent(viewHolder.feedSurfaceCoordinator.getView());
        }

        viewHolder.isShown = shown;
    }

    private static void setOverview(
            ViewHolder viewHolder, boolean isShowingOverview, boolean wasExploreSurfaceVisible) {
        if (isShowingOverview) {
            setVisibility(viewHolder, wasExploreSurfaceVisible);
        } else {
            setVisibility(viewHolder, false);
            destroy(viewHolder);
        }
    }

    private static void destroy(ViewHolder viewHolder) {
        if (viewHolder.feedSurfaceCoordinator != null) viewHolder.feedSurfaceCoordinator.destroy();
        viewHolder.feedSurfaceCoordinator = null;
    }
}
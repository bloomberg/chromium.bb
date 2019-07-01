// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.Rect;
import android.support.annotation.NonNull;

import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

/**
 * Interface for the Grid Tab Switcher.
 */
public interface GridTabSwitcher {
    // TODO(960196): Remove the following interfaces when the associated bug is resolved.
    /**
     * An observer that is notified when the GridTabSwitcher view state changes.
     */
    interface GridOverviewModeObserver {
        /**
         * Called when overview mode starts showing.
         */
        void startedShowing();

        /**
         * Called when overview mode finishes showing.
         */
        void finishedShowing();

        /**
         * Called when overview mode starts hiding.
         */
        void startedHiding();

        /**
         * Called when overview mode finishes hiding.
         */
        void finishedHiding();
    }

    /**
     * Interface to control the GridTabSwitcher.
     */
    interface GridController {
        /**
         * @return Whether or not the overview {@link Layout} is visible.
         */
        boolean overviewVisible();

        /**
         * @param listener Registers {@code listener} for overview mode status changes.
         */
        void addOverviewModeObserver(GridOverviewModeObserver listener);

        /**
         * @param listener Unregisters {@code listener} for overview mode status changes.
         */
        void removeOverviewModeObserver(GridOverviewModeObserver listener);

        /**
         * Hide the overview.
         * @param animate Whether we should animate while hiding.
         */
        void hideOverview(boolean animate);

        /**
         * Show the overview.
         * @param animate Whether we should animate while showing.
         */
        void showOverview(boolean animate);

        /**
         * Called by the GridTabSwitcherLayout when the system back button is pressed.
         * @return Whether or not the GridTabSwitcher consumed the event.
         */
        boolean onBackPressed();
    }

    /**
     * @return GridController implementation that can be used for controlling
     *         grid visibility changes.
     */
    GridController getGridController();

    /**
     * @return The dynamic resource ID of the GridTabSwitcher RecyclerView.
     */
    int getResourceId();

    /**
     * @return The timestamp of last dirty event of {@link ViewResourceAdapter} of
     * {@link TabListRecyclerView}.
     */
    long getLastDirtyTimeForTesting();

    /**
     * Before calling {@link GridController#showOverview} to start showing the
     * GridTabSwitcher {@link TabListRecyclerView}, call this to populate it without making it
     * visible.
     * @return Whether the {@link TabListRecyclerView} can be shown quickly.
     */
    boolean prepareOverview();

    /**
     * This is called after the compositor animation is done, for potential clean-up work.
     * {@link GridOverviewModeObserver#finishedHiding} happens after
     * the Android View animation, but before the compositor animation.
     */
    void postHiding();

    /**
     * @param forceUpdate Whether to measure the current location again. If not, return the last
     *                    location measured on last layout, which can be wrong after scrolling.
     * @return The {@link Rect} of the thumbnail of the current tab, relative to the
     *         GridTabSwitcher {@link TabListRecyclerView} coordinates.
     */
    @NonNull
    Rect getThumbnailLocationOfCurrentTab(boolean forceUpdate);
}

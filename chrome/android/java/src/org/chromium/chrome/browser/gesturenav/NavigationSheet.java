// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.content.Context;
import android.view.View;

import org.chromium.base.Supplier;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.NavigationHistory;

/**
 * Interface that defines the methods for controlling Navigation sheet.
 */
public interface NavigationSheet {
    // Field trial variation key that enables navigation sheet.
    static final String NAVIGATION_SHEET_ENABLED_KEY = "overscroll_history_navigation_bottom_sheet";

    /**
     * Delegate performing navigation-related operations/providing the required info.
     */
    interface Delegate {
        /**
         * @param {@code true} if the requested history is of forward navigation.
         * @return {@link NavigationHistory} object.
         */
        NavigationHistory getHistory(boolean forward);

        /**
         * Navigates to the page associated with the given index.
         */
        void navigateToIndex(int index);

        /**
         * Sets a runnable to execute when the associated Tab is closed.
         * @param runnable Runnable to execute.
         */
        void setTabCloseRunnable(Runnable runnable);
    }

    /**
     * Create {@link NavigationSheet} object.
     * @param rootView Root view whose dimension is used for the sheet.
     * @param context {@link Context} used to retrieve resources.
     * @param bottomSheetController {@link BottomSheetController} object.
     * @param delegate Delegate used by navigation sheet to perform actions.
     * @return NavigationSheet object.
     */
    public static NavigationSheet create(View rootView, Context context,
            Supplier<BottomSheetController> bottomSheetController,
            NavigationSheet.Delegate delegate) {
        return new NavigationSheetCoordinator(rootView, context, bottomSheetController, delegate);
    }

    /**
     * @return {@code true} if navigation sheet is enabled.
     */
    static boolean isEnabled() {
        return ChromeFeatureList.isInitialized()
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.OVERSCROLL_HISTORY_NAVIGATION,
                        NAVIGATION_SHEET_ENABLED_KEY, false);
    }

    /**
     * @return {@code true} if another instance of NavigationSheet is already showing.
     */
    public static boolean isInstanceShowing(BottomSheetController controller) {
        if (controller == null) return false;
        BottomSheet sheet = controller.getBottomSheet();
        return (sheet.getCurrentSheetContent() instanceof NavigationSheetCoordinator)
                && sheet.isSheetOpen();
    }

    /**
     * Dummy object that does nothing. Saves lots of null checks.
     */
    static final NavigationSheet DUMMY = new NavigationSheet() {
        @Override
        public void start(boolean forward, boolean showCloseIndicator) {}

        @Override
        public void startAndExpand(boolean forward, boolean animate) {}

        @Override
        public void onScroll(float delta, float overscroll, boolean willNavigate) {}

        @Override
        public void release() {}

        @Override
        public boolean isHidden() {
            return true;
        }

        @Override
        public boolean isExpanded() {
            return false;
        }
    };

    /**
     * Get the navigation sheet ready as the gesture starts.
     * @param forward {@code true} if we're navigating forward.
     * @param showCloseIndicator {@code true} if we should show 'close chrome' indicator
     *        on the arrow puck.
     */
    void start(boolean forward, boolean showCloseIndicator);

    /**
     * Fully expand the navigation sheet from the beginning.
     * @param forward {@code true} if this is for forward navigation.
     * @param animate {@code true} to enable animation.
     */
    void startAndExpand(boolean forward, boolean animate);

    /**
     * Process swipe gesture and update the navigation sheet state.
     * @param delta Scroll delta from the previous scroll.
     * @param overscroll Total amount of scroll since the dragging started.
     * @param willNavigate {@code true} if navgation will be triggered upon release.
     */
    void onScroll(float delta, float overscroll, boolean willNavigate);

    /**
     * Process release events.
     */
    void release();

    /**
     * @param {@code true} if navigation sheet is in hidden state.
     */
    boolean isHidden();

    /**
     * @param {@code true} if navigation sheet is in expanded (half/full) state.
     */
    boolean isExpanded();
}

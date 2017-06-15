// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.BottomToolbarPhone;

/**
 * A class that handles showing and hiding the Chrome Home new tab UI.
 *
 * TODO(twellington): Add tests for this class.
 */
public class BottomSheetNewTabController extends EmptyBottomSheetObserver {
    private final BottomSheet mBottomSheet;
    private final BottomToolbarPhone mToolbar;

    private LayoutManagerChrome mLayoutManager;
    private OverviewModeObserver mOverviewModeObserver;
    private TabModelSelector mTabModelSelector;

    private boolean mIsShowingNewTabUi;
    private boolean mIsShowingNormalToolbar;

    /**
     * Creates a new {@link BottomSheetNewTabController}.
     * @param bottomSheet The {@link BottomSheet} that will be opened as part of the new tab UI.
     * @param toolbar The {@link BottomToolbarPhone} that this controller will set state on as part
     *                of the new tab UI.
     */
    public BottomSheetNewTabController(BottomSheet bottomSheet, BottomToolbarPhone toolbar) {
        mBottomSheet = bottomSheet;
        mBottomSheet.addObserver(this);
        mToolbar = toolbar;
    }

    /**
     * @param tabModelSelector A TabModelSelector for getting the current tab and activity.
     */
    public void setTabModelSelector(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
    }

    /**
     * @param layoutManager The {@link LayoutManagerChrome} used to show and hide overview mode.
     */
    public void setLayoutManagerChrome(LayoutManagerChrome layoutManager) {
        assert mLayoutManager == null;

        mLayoutManager = layoutManager;

        mOverviewModeObserver = new EmptyOverviewModeObserver() {
            @Override
            public void onOverviewModeStartedHiding(boolean showToolbar, boolean delayAnimation) {
                if (!mIsShowingNewTabUi
                        || mBottomSheet.getTargetSheetState() == BottomSheet.SHEET_STATE_PEEK) {
                    return;
                }

                // Close the bottom sheet to hide the new tab UI.
                mBottomSheet.setSheetState(BottomSheet.SHEET_STATE_PEEK, true);
            }
        };
        mLayoutManager.addOverviewModeObserver(mOverviewModeObserver);
    }

    /**
     * Shows the new tab UI.
     * @param isIncognito Whether to display the incognito new tab UI.
     */
    public void displayNewTabUi(boolean isIncognito) {
        mIsShowingNewTabUi = true;

        // Tell the model that a new tab may be added soon.
        mTabModelSelector.getModel(isIncognito).setIsPendingTabAdd(true);

        // Select the correct model, immediately ending animations so that the sheet content is not
        // in transition while the sheet is opening.
        if (mTabModelSelector.isIncognitoSelected() != isIncognito) {
            mTabModelSelector.selectModel(isIncognito);
            mBottomSheet.endTransitionAnimations();
        }

        // Show the tab switcher if needed. The overview should be shown before the sheet is opened
        // to ensure the toolbar ends up in the correct state.
        if (!mLayoutManager.overviewVisible()) mLayoutManager.showOverview(true);

        // Open the sheet if it isn't open already. When toggling accessibility mode, this method
        // may get called while the sheet is open. In that case, we should finish showing the new
        // tab UI.
        if (!mBottomSheet.isSheetOpen()) {
            mBottomSheet.setSheetState(mTabModelSelector.getTotalTabCount() == 0
                            ? BottomSheet.SHEET_STATE_FULL
                            : BottomSheet.SHEET_STATE_HALF,
                    true);
            mBottomSheet.getBottomSheetMetrics().recordSheetOpenReason(
                    BottomSheetMetrics.OPENED_BY_NEW_TAB_CREATION);
        }

        // Transition from the tab switcher toolbar to the normal toolbar.
        mToolbar.showNormalToolbar();
        mIsShowingNormalToolbar = true;
    }

    /**
     * @return Whether the the new tab UI is showing.
     */
    public boolean isShowingNewTabUi() {
        return mIsShowingNewTabUi;
    }

    /**
     * @return Whether the Google 'G' logo should be shown in the location bar.
     */
    public boolean shouldShowGoogleGInLocationBar() {
        return mIsShowingNewTabUi
                && mBottomSheet.getTargetSheetState() != BottomSheet.SHEET_STATE_PEEK
                && !mTabModelSelector.isIncognitoSelected()
                && TemplateUrlService.getInstance().isDefaultSearchEngineGoogle();
    }

    @Override
    public void onLoadUrl(String url) {
        if (!mIsShowingNewTabUi) return;

        mLayoutManager.hideOverview(true);

        // BottomSheet closes itself when URLs are loaded; wait to finish hiding the new tab UI
        // until the bottom sheet is closed.
    }

    @Override
    public void onSheetReleased() {
        if (!mIsShowingNewTabUi) return;

        // Start transitioning back to the tab switcher toolbar when the sheet is released to help
        // smooth out animations.
        if (mBottomSheet.getTargetSheetState() == BottomSheet.SHEET_STATE_PEEK
                && mIsShowingNormalToolbar) {
            mIsShowingNormalToolbar = false;
            mToolbar.showTabSwitcherToolbar();
        }
    }

    @Override
    public void onSheetOffsetChanged(float heightFraction) {
        if (!mIsShowingNewTabUi) return;

        // Start transitioning to the tab switcher toolbar when the sheet is close to the bottom
        // of the screen.
        if (mIsShowingNormalToolbar && heightFraction < 0.2f
                && mBottomSheet.getTargetSheetState() == BottomSheet.SHEET_STATE_PEEK
                && mLayoutManager.overviewVisible()) {
            mIsShowingNormalToolbar = false;
            mToolbar.showTabSwitcherToolbar();
        }
    }

    @Override
    public void onSheetClosed() {
        if (!mIsShowingNewTabUi) return;

        mIsShowingNewTabUi = false;

        // If the incognito tab model is showing, but has no tabs, this indicates that the model
        // was switched during the creation of an incognito ntp and the user closed the sheet
        // without loading a URL. Switch back to the normal model.
        if (mTabModelSelector.isIncognitoSelected()
                && mTabModelSelector.getModel(true).getCount() == 0) {
            mTabModelSelector.selectModel(false);
        }

        onNewTabUiHidden();
    }

    /** Called after the new tab UI is hidden. Resets properties on the tab models. */
    private void onNewTabUiHidden() {
        mTabModelSelector.getModel(false).setIsPendingTabAdd(false);
        mTabModelSelector.getModel(true).setIsPendingTabAdd(false);
    }
}

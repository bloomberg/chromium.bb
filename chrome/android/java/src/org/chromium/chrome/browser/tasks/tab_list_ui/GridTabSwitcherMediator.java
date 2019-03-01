// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_list_ui;

import static org.chromium.chrome.browser.tasks.tab_list_ui.TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES;
import static org.chromium.chrome.browser.tasks.tab_list_ui.TabListContainerProperties.BOTTOM_CONTROLS_HEIGHT;
import static org.chromium.chrome.browser.tasks.tab_list_ui.TabListContainerProperties.INITIAL_SCROLL_INDEX;
import static org.chromium.chrome.browser.tasks.tab_list_ui.TabListContainerProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.tab_list_ui.TabListContainerProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_list_ui.TabListContainerProperties.TOP_CONTROLS_HEIGHT;
import static org.chromium.chrome.browser.tasks.tab_list_ui.TabListContainerProperties.VISIBILITY_LISTENER;

import org.chromium.chrome.browser.compositor.layouts.OverviewModeController;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * The Mediator that is responsible for resetting the tab grid based on visibility and model
 * changes.
 */
class GridTabSwitcherMediator
        implements OverviewModeController, TabListRecyclerView.VisibilityListener {
    // This should be the same as TabListCoordinator.GRID_LAYOUT_SPAN_COUNT for the selected tab
    // to be on the 2nd row.
    private static final int INITIAL_SCROLL_INDEX_OFFSET = 2;

    private final GridTabSwitcherCoordinator mCoordinator;
    private final PropertyModel mContainerViewModel;
    private final TabModelSelector mTabModelSelector;
    private final TabModelSelectorTabModelObserver mTabModelObserver;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private final List<OverviewModeObserver> mObservers = new ArrayList<>();
    private final ChromeFullscreenManager mFullscreenManager;
    private final ChromeFullscreenManager.FullscreenListener mFullscreenListener =
            new ChromeFullscreenManager.FullscreenListener() {
                @Override
                public void onContentOffsetChanged(int offset) {}

                @Override
                public void onControlsOffsetChanged(
                        int topOffset, int bottomOffset, boolean needsAnimate) {}

                @Override
                public void onToggleOverlayVideoMode(boolean enabled) {}

                @Override
                public void onBottomControlsHeightChanged(int bottomControlsHeight) {
                    mContainerViewModel.set(BOTTOM_CONTROLS_HEIGHT, bottomControlsHeight);
                }
            };

    /**
     * In cases where a didSelectTab was due to closing a tab or switching models with a toggle,
     * we don't change tab grid visibility.
     */
    private boolean mShouldIgnoreNextSelect;

    /**
     * Basic constructor for the Mediator.
     * @param coordinator The {@link GridTabSwitcherCoordinator} that owns this Mediator.
     * @param containerViewModel The {@link PropertyModel} to keep state on the View containing the
     *         grid.
     * @param tabModelSelector {@link TabModelSelector} to observer for model and selection changes.
     * @param fullscreenManager {@link FullscreenManager} to use.
     */
    GridTabSwitcherMediator(GridTabSwitcherCoordinator coordinator,
            PropertyModel containerViewModel, TabModelSelector tabModelSelector,
            ChromeFullscreenManager fullscreenManager) {
        mCoordinator = coordinator;
        mContainerViewModel = containerViewModel;
        mTabModelSelector = tabModelSelector;
        mFullscreenManager = fullscreenManager;

        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                mShouldIgnoreNextSelect = true;
                mCoordinator.resetWithTabModel(newModel);
                mContainerViewModel.set(IS_INCOGNITO, newModel.isIncognito());
            }
        };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);

        mTabModelSelector.setOverviewModeBehavior(this);

        mTabModelObserver = new TabModelSelectorTabModelObserver(mTabModelSelector) {

            @Override
            public void didSelectTab(Tab tab, int type, int lastId) {
                if (mShouldIgnoreNextSelect) {
                    mShouldIgnoreNextSelect = false;
                    return;
                }
                setVisibility(false);
            }

            @Override
            public void willCloseTab(Tab tab, boolean animate) {
                // Needed to handle the extra select for closing the currently selected tab.
                mShouldIgnoreNextSelect = true;
            }

            @Override
            public void didCloseTab(int tabId, boolean incognito) {
                // Handles most of the tab closes.
                mShouldIgnoreNextSelect = true;
            }
        };

        mFullscreenManager.addListener(mFullscreenListener);

        mContainerViewModel.set(VISIBILITY_LISTENER, this);
        mContainerViewModel.set(IS_INCOGNITO, mTabModelSelector.getCurrentModel().isIncognito());
        mContainerViewModel.set(ANIMATE_VISIBILITY_CHANGES, true);
        mContainerViewModel.set(TOP_CONTROLS_HEIGHT, fullscreenManager.getTopControlsHeight());
        mContainerViewModel.set(
                BOTTOM_CONTROLS_HEIGHT, fullscreenManager.getBottomControlsHeight());
    }

    private void setVisibility(boolean isVisible) {
        if (isVisible) {
            mCoordinator.resetWithTabModel(mTabModelSelector.getCurrentModel());
            int initialPosition = Math.max(
                    mTabModelSelector.getCurrentModel().index() - INITIAL_SCROLL_INDEX_OFFSET, 0);
            mContainerViewModel.set(INITIAL_SCROLL_INDEX, initialPosition);
        }

        mContainerViewModel.set(IS_VISIBLE, isVisible);
    }

    @Override
    public boolean overviewVisible() {
        return mContainerViewModel.get(IS_VISIBLE);
    }

    @Override
    public void addOverviewModeObserver(OverviewModeObserver listener) {
        mObservers.add(listener);
    }

    @Override
    public void removeOverviewModeObserver(OverviewModeObserver listener) {
        mObservers.remove(listener);
    }

    @Override
    public void hideOverview(boolean animate) {
        if (!animate) mContainerViewModel.set(ANIMATE_VISIBILITY_CHANGES, false);
        setVisibility(false);
        mContainerViewModel.set(ANIMATE_VISIBILITY_CHANGES, true);
    }

    @Override
    public void showOverview(boolean animate) {
        if (!animate) mContainerViewModel.set(ANIMATE_VISIBILITY_CHANGES, false);
        setVisibility(true);
        mContainerViewModel.set(ANIMATE_VISIBILITY_CHANGES, true);
    }

    @Override
    public void startedShowing() {
        for (OverviewModeObserver observer : mObservers) {
            observer.onOverviewModeStartedShowing(true);
        }
    }

    @Override
    public void finishedShowing() {
        for (OverviewModeObserver observer : mObservers) {
            observer.onOverviewModeFinishedShowing();
        }
    }

    @Override
    public void startedHiding() {
        for (OverviewModeObserver observer : mObservers) {
            observer.onOverviewModeStartedHiding(true, false);
        }
    }

    @Override
    public void finishedHiding() {
        mCoordinator.resetWithTabModel(null);
        mContainerViewModel.set(INITIAL_SCROLL_INDEX, 0);
        for (OverviewModeObserver observer : mObservers) {
            observer.onOverviewModeFinishedHiding();
        }
    }

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        mFullscreenManager.removeListener(mFullscreenListener);
        mTabModelObserver.destroy();
    }
}

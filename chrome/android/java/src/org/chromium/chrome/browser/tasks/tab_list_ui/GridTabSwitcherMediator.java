// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_list_ui;

import static org.chromium.chrome.browser.tasks.tab_list_ui.TabListContainerProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.tab_list_ui.TabListContainerProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_list_ui.TabListContainerProperties.VISIBILITY_LISTENER;

import android.view.View;

import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
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
        implements OverviewModeBehavior, TabListRecyclerView.VisibilityListener {
    private final GridTabSwitcherCoordinator mCoordinator;
    private final PropertyModel mContainerViewModel;
    private final TabModelSelector mTabModelSelector;
    private final TabModelSelectorTabModelObserver mTabModelObserver;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private final List<OverviewModeObserver> mObservers = new ArrayList<>();

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
     */
    GridTabSwitcherMediator(GridTabSwitcherCoordinator coordinator,
            PropertyModel containerViewModel, TabModelSelector tabModelSelector) {
        mCoordinator = coordinator;
        mContainerViewModel = containerViewModel;
        mTabModelSelector = tabModelSelector;

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

        mContainerViewModel.set(VISIBILITY_LISTENER, this);
        mContainerViewModel.set(IS_INCOGNITO, mTabModelSelector.getCurrentModel().isIncognito());
    }

    /**
     * @return The {@link android.view.View.OnClickListener} to override for the tab switcher
     *         button.
     */
    View.OnClickListener getTabSwitcherButtonClickListener() {
        return view -> setVisibility(!mContainerViewModel.get(IS_VISIBLE));
    }

    private void setVisibility(boolean isVisible) {
        if (isVisible) mCoordinator.resetWithTabModel(mTabModelSelector.getCurrentModel());
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
        for (OverviewModeObserver observer : mObservers) {
            observer.onOverviewModeFinishedHiding();
        }
    }

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        mTabModelObserver.destroy();
    }
}

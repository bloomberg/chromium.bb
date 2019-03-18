// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * A mediator for the TabGroupUi. Responsible for managing the
 * internal state of the component.
 */
public class TabGroupUiMediator implements Destroyable {
    /**
     * Defines an interface for a {@link TabGroupUiMediator} reset event
     * handler.
     */
    interface ResetHandler {
        /**
         * Handles a reset event originated from {@link TabGroupUiMediator}
         * when the bottom sheet is collapsed.
         *
         * @param tabs List of Tabs to reset.
         */
        void resetStripWithListOfTabs(List<Tab> tabs);

        /**
         * Handles a reset event originated from {@link TabGroupUiMediator}
         * when the bottom sheet is expanded.
         *
         * @param tabs List of Tabs to reset.
         */
        void resetSheetWithListOfTabs(List<Tab> tabs);
    }

    private final PropertyModel mToolbarPropertyModel;
    private final TabModelObserver mTabModelObserver;
    private final ResetHandler mResetHandler;
    private final TabModelSelector mTabModelSelector;
    private final TabCreatorManager mTabCreatorManager;
    private final OverviewModeBehavior mOverviewModeBehavior;
    private final OverviewModeBehavior.OverviewModeObserver mOverviewModeObserver;
    private final BottomControlsCoordinator
            .BottomControlsVisibilityController mVisibilityController;

    TabGroupUiMediator(
            BottomControlsCoordinator.BottomControlsVisibilityController visibilityController,
            ResetHandler resetHandler, PropertyModel toolbarPropertyModel,
            TabModelSelector tabModelSelector, TabCreatorManager tabCreatorManager,
            OverviewModeBehavior overviewModeBehavior) {
        mResetHandler = resetHandler;
        mToolbarPropertyModel = toolbarPropertyModel;
        mTabModelSelector = tabModelSelector;
        mTabCreatorManager = tabCreatorManager;
        mOverviewModeBehavior = overviewModeBehavior;
        mVisibilityController = visibilityController;

        // register for tab model
        mTabModelObserver = new EmptyTabModelObserver() {
            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                if (getRelatedTabsForId(lastId).contains(tab)) return;
                resetTabStripWithRelatedTabsForId(tab.getId());
            }

            @Override
            public void didAddTab(Tab tab, int type) {
                if (type == TabLaunchType.FROM_CHROME_UI) return;
                resetTabStripWithRelatedTabsForId(tab.getId());
            }
        };
        mOverviewModeObserver = new EmptyOverviewModeObserver() {
            @Override
            public void onOverviewModeStartedShowing(boolean showToolbar) {
                resetTabStripWithRelatedTabsForId(Tab.INVALID_TAB_ID);
            }

            @Override
            public void onOverviewModeFinishedHiding() {
                Tab tab = mTabModelSelector.getCurrentTab();
                if (tab == null) return;
                resetTabStripWithRelatedTabsForId(tab.getId());
            }
        };

        mTabModelSelector.getTabModelFilterProvider().addTabModelFilterObserver(mTabModelObserver);
        mOverviewModeBehavior.addOverviewModeObserver(mOverviewModeObserver);
        setupToolbarClickHandlers();
        mToolbarPropertyModel.set(TabStripToolbarViewProperties.IS_MAIN_CONTENT_VISIBLE, true);
        Tab tab = mTabModelSelector.getCurrentTab();
        if (tab != null) {
            resetTabStripWithRelatedTabsForId(tab.getId());
        }
    }

    private void setupToolbarClickHandlers() {
        mToolbarPropertyModel.set(TabStripToolbarViewProperties.EXPAND_CLICK_LISTENER, view -> {
            Tab currentTab = mTabModelSelector.getCurrentTab();
            if (currentTab == null) return;
            mResetHandler.resetSheetWithListOfTabs(getRelatedTabsForId(currentTab.getId()));
        });
        mToolbarPropertyModel.set(TabStripToolbarViewProperties.ADD_CLICK_LISTENER, view -> {
            Tab currentTab = mTabModelSelector.getCurrentTab();
            mTabCreatorManager.getTabCreator(currentTab.isIncognito())
                    .createNewTab(new LoadUrlParams(UrlConstants.NTP_URL), TabLaunchType.FROM_LINK,
                            currentTab);
        });
    }

    private void resetTabStripWithRelatedTabsForId(int id) {
        List<Tab> listOfTabs = mTabModelSelector.getTabModelFilterProvider()
                                       .getCurrentTabModelFilter()
                                       .getRelatedTabList(id);

        if (listOfTabs == null || listOfTabs.size() < 2) {
            mResetHandler.resetStripWithListOfTabs(null);
            mVisibilityController.setBottomControlsVisible(false);
        } else {
            mResetHandler.resetStripWithListOfTabs(listOfTabs);
            mVisibilityController.setBottomControlsVisible(true);
        }
    }

    private List<Tab> getRelatedTabsForId(int id) {
        return mTabModelSelector.getTabModelFilterProvider()
                .getCurrentTabModelFilter()
                .getRelatedTabList(id);
    }

    @Override
    public void destroy() {
        if (mTabModelObserver != null && mTabModelSelector != null) {
            mTabModelSelector.getTabModelFilterProvider().removeTabModelFilterObserver(
                    mTabModelObserver);
        }
        mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
    }
}

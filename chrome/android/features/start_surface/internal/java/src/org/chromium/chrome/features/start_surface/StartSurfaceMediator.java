// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BottomBarClickListener;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_EXPLORE_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_INCOGNITO;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;

import android.support.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.GridTabSwitcher;
import org.chromium.ui.modelutil.PropertyModel;

/** The mediator implements the logic to interact with the surfaces and caller. */
class StartSurfaceMediator
        implements StartSurface.Controller, GridTabSwitcher.GridOverviewModeObserver {
    private final ObserverList<StartSurface.OverviewModeObserver> mObservers = new ObserverList<>();
    private final GridTabSwitcher.GridController mGridController;
    @Nullable
    private final PropertyModel mPropertyModel;

    StartSurfaceMediator(GridTabSwitcher.GridController gridController,
            @Nullable PropertyModel propertyModel, TabModelSelector tabModelSelector) {
        mGridController = gridController;
        mPropertyModel = propertyModel;

        if (mPropertyModel != null) {
            mPropertyModel.set(
                    BottomBarClickListener, new StartSurfaceProperties.BottomBarClickListener() {
                        // TODO(crbug.com/982018): Animate switching between explore and home
                        // surface.
                        @Override
                        public void onHomeButtonClicked() {
                            mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, false);
                        }

                        @Override
                        public void onExploreButtonClicked() {
                            // TODO(crbug.com/982018): Hide the Tab switcher toolbar when showing
                            // explore surface.
                            mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, true);
                        }
                    });

            tabModelSelector.addObserver(new EmptyTabModelSelectorObserver() {
                @Override
                public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                    mPropertyModel.set(IS_INCOGNITO, newModel.isIncognito());
                }
            });

            // Set the initial state.
            mPropertyModel.set(IS_INCOGNITO, tabModelSelector.isIncognitoSelected());
        }

        mGridController.addOverviewModeObserver(this);
    }

    // Implements StartSurface.Controller
    @Override
    public boolean overviewVisible() {
        return mGridController.overviewVisible();
    }

    @Override
    public void addOverviewModeObserver(StartSurface.OverviewModeObserver observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeOverviewModeObserver(StartSurface.OverviewModeObserver observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void hideOverview(boolean animate) {
        mGridController.hideOverview(animate);
    }

    @Override
    public void showOverview(boolean animate) {
        mGridController.showOverview(animate);

        // TODO(crbug.com/982018): Animate the bottom bar together with the Tab Grid view.
        if (mPropertyModel != null) mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
    }

    @Override
    public boolean onBackPressed() {
        // TODO(crbug.com/982018): Check whether explore surface is shown, if yes, switch back to
        // home surface.
        return mGridController.onBackPressed();
    }

    // Implements GridTabSwitcher.GridOverviewModeObserver.
    @Override
    public void startedShowing() {
        for (StartSurface.OverviewModeObserver observer : mObservers) {
            observer.startedShowing();
        }
    }

    @Override
    public void finishedShowing() {
        for (StartSurface.OverviewModeObserver observer : mObservers) {
            observer.finishedShowing();
        }
    }

    @Override
    public void startedHiding() {
        if (mPropertyModel != null) mPropertyModel.set(IS_SHOWING_OVERVIEW, false);
        for (StartSurface.OverviewModeObserver observer : mObservers) {
            observer.startedHiding();
        }
    }

    @Override
    public void finishedHiding() {
        for (StartSurface.OverviewModeObserver observer : mObservers) {
            observer.finishedHiding();
        }
    }
}
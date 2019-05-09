// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider.SYNTHETIC_TRIAL_POSTFIX;

import android.view.ViewGroup;

import org.chromium.base.annotations.UsedByReflection;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.metrics.UmaSessionStats;

/**
 * Impl class that will resolve components for tab management.
 */
@UsedByReflection("TabManagementModuleProvider.java")
public class TabManagementModuleImpl implements TabManagementModule {
    @Override
    public GridTabSwitcher createGridTabSwitcher(ChromeActivity activity) {
        if (ChromeFeatureList.isInitialized()) {
            UmaSessionStats.registerSyntheticFieldTrial(
                    ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + SYNTHETIC_TRIAL_POSTFIX,
                    "Downloaded_Enabled");
        }
        return new GridTabSwitcherCoordinator(activity, activity.getLifecycleDispatcher(),
                activity.getToolbarManager(), activity.getTabModelSelector(),
                activity.getTabContentManager(), activity.getCompositorViewHolder(),
                activity.getFullscreenManager(), activity, activity::onBackPressed);
    }

    @Override
    public TabGroupUi createTabGroupUi(
            ViewGroup parentView, ThemeColorProvider themeColorProvider) {
        return new TabGroupUiCoordinator(parentView, themeColorProvider);
    }
}

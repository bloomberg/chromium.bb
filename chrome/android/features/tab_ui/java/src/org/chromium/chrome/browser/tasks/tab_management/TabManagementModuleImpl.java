// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.ViewGroup;

import org.chromium.base.annotations.UsedByReflection;
import org.chromium.chrome.browser.ChromeActivity;

/**
 * Impl class that will resolve components for tab management.
 */
@UsedByReflection("TabManagementModuleProvider.java")
public class TabManagementModuleImpl implements TabManagementModule {
    @Override
    public GridTabSwitcher createGridTabSwitcher(ChromeActivity activity) {
        return new GridTabSwitcherCoordinator(activity, activity.getLifecycleDispatcher(),
                activity.getToolbarManager(), activity.getTabModelSelector(),
                activity.getTabContentManager(), activity.getCompositorViewHolder(),
                activity.getFullscreenManager());
    }

    @Override
    public TabGroupUi createTabGroupUi(ViewGroup parentView) {
        return new TabGroupUiCoordinator(parentView);
    }
}

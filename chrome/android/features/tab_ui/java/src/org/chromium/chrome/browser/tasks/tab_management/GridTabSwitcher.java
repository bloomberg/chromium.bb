// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeController;

/**
 * Interface for the Grid Tab Switcher.
 */
public interface GridTabSwitcher {
    // TODO(960196): Remove the following interfaces when the associated bug is resolved.

    /**
     * Interface to control the GridTabSwitcher.
     */
    interface GridController extends OverviewModeController {}

    /**
     * An observer that is notified when the GridTabSwitcher view state changes.
     */
    interface GridVisibilityObserver extends OverviewModeBehavior.OverviewModeObserver {}

    GridController getGridController();
}

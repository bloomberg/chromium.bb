// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import org.chromium.base.Supplier;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Implementation of {@link NavigationHandler#ActionDelegate} that works for Tab switcher.
 * Swipe from left exits the tab switcher and goes back to the current tab. Can exit
 * Chrome app itself if there's no current tab.
 */
public class TabSwitcherActionDelegate implements NavigationHandler.ActionDelegate {
    private final Supplier<Tab> mCurrentTab;
    private final ChromeActivity mActivity;

    public TabSwitcherActionDelegate(Supplier<Tab> currentTab) {
        mCurrentTab = currentTab;

        // Cache the activity at the beginning since it may not be reachable
        // later when current tab becomes null.
        mActivity = currentTab.get().getActivity();
    }

    @Override
    public boolean canNavigate(boolean forward) {
        return !forward;
    }

    @Override
    public void navigate(boolean forward) {
        assert !forward : "Should be called only for back navigation";
        mActivity.onBackPressed();
    }

    @Override
    public boolean willBackExitApp() {
        return mCurrentTab.get() == null;
    }
}

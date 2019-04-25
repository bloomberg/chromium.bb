// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Implementation of {@link NavigationHandler#ActionDelegate} that works with
 * native/rendered pages in tabbed mode. Uses interface methods of {@link Tab}
 * and {@link ChromeActivity} to implement navigation.
 */
public class TabbedActionDelegate implements NavigationHandler.ActionDelegate {
    private final Tab mTab;
    private final ChromeActivity mActivity;

    public TabbedActionDelegate(Tab tab) {
        mTab = tab;
        mActivity = tab.getActivity();
    }

    @Override
    public boolean canNavigate(boolean forward) {
        return forward ? mTab.canGoForward() : true;
    }

    @Override
    public void navigate(boolean forward) {
        if (forward) {
            mTab.goForward();
        } else {
            mActivity.onBackPressed();
        }
    }

    @Override
    public boolean willBackExitApp() {
        return !mTab.canGoBack() && !mTab.getActivity().backShouldCloseTab(mTab);
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.TabHidingType;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.TouchlessEventHandler;

/**
 * Bridge to org.chromium.ui.base.TouchlessEventHandler
 */
public class TouchlessTabObserver extends EmptyTabObserver {
    @Override
    public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
        // Only want to observer the navigation in main frame and not fragment
        // navigation.
        if (navigation.isInMainFrame() && !navigation.isFragmentNavigation()) {
            TouchlessEventHandler.onDidFinishNavigation();
        }
    }

    @Override
    public void onHidden(Tab tab, @TabHidingType int type) {
        // Only want to observer the activity hidden.
        if (type == Tab.TabHidingType.ACTIVITY_HIDDEN) {
            TouchlessEventHandler.onActivityHidden();
        }
    }
}
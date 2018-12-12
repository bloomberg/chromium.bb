// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * Class that observes url and tab changes in order to track when browsing stops and starts for each
 * visited fdqn.
 */
public class PageViewObserver {
    private TabModelSelector mTabModelSelector;
    private EventTracker mEventTracker;

    public PageViewObserver(TabModelSelector tabModelSelector, EventTracker eventTracker) {
        mTabModelSelector = tabModelSelector;
        mEventTracker = eventTracker;
    }

    // TODO(pnoland): implement page view observation and push events to EventTracker.
}

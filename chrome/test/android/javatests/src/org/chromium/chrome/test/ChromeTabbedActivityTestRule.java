// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.view.View;

import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.ChromeTabbedActivityTestCommon.ChromeTabbedActivityTestCommonCallback;

/** Custom ActivityTestRule for test using ChromeTabbedActivity */
public class ChromeTabbedActivityTestRule extends ChromeActivityTestRule<ChromeTabbedActivity>
        implements ChromeTabbedActivityTestCommonCallback {
    private final ChromeTabbedActivityTestCommon mTestCommon;

    public ChromeTabbedActivityTestRule() {
        super(ChromeTabbedActivity.class);
        mTestCommon = new ChromeTabbedActivityTestCommon(this);
    }

    /**
     * Load a url in multiple new tabs in parallel. Each {@link Tab} will pretend to be
     * created from a link.
     *
     * @param url The url of the page to load.
     * @param numTabs The number of tabs to open.
     */
    public void loadUrlInManyNewTabs(final String url, final int numTabs)
            throws InterruptedException {
        mTestCommon.loadUrlInManyNewTabs(url, numTabs);
    }

    /**
     * Long presses the view, selects an item from the context menu, and
     * asserts that a new tab is opened and is incognito if expectIncognito is true.
     * @param view The View to long press.
     * @param contextMenuItemId The context menu item to select on the view.
     * @param expectIncognito Whether the opened tab is expected to be incognito.
     * @param expectedUrl The expected url for the new tab.
     */
    public void invokeContextMenuAndOpenInANewTab(View view, int contextMenuItemId,
            boolean expectIncognito, final String expectedUrl) throws InterruptedException {
        mTestCommon.invokeContextMenuAndOpenInANewTab(
                view, contextMenuItemId, expectIncognito, expectedUrl);
    }
}

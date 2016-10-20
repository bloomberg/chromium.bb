// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.text.TextUtils;
import android.view.View;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.content.browser.test.util.TestTouchUtils;

import java.util.concurrent.TimeoutException;

/**
 * The base class of the ChromeTabbedActivity specific tests. It provides the common methods
 * to access the ChromeTabbedActivity UI.
 */
public abstract class ChromeTabbedActivityTestBase extends
        ChromeActivityTestCaseBase<ChromeTabbedActivity> {
    private static final String TAG = "ChromeTabbedActivityTestBase";

    public ChromeTabbedActivityTestBase() {
        super(ChromeTabbedActivity.class);
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
        final CallbackHelper[] pageLoadedCallbacks = new CallbackHelper[numTabs];
        final int[] tabIds = new int[numTabs];
        for (int i = 0; i < numTabs; ++i) {
            final int index = i;
            getInstrumentation().runOnMainSync(new Runnable() {
                @Override
                public void run() {
                    Tab currentTab = getActivity().getCurrentTabCreator().launchUrl(
                            url, TabLaunchType.FROM_LINK);
                    final CallbackHelper pageLoadCallback = new CallbackHelper();
                    pageLoadedCallbacks[index] = pageLoadCallback;
                    currentTab.addObserver(new EmptyTabObserver() {
                        @Override
                        public void onPageLoadFinished(Tab tab) {
                            pageLoadCallback.notifyCalled();
                            tab.removeObserver(this);
                        }
                    });
                    tabIds[index] = currentTab.getId();
                }
            });
        }
        //  When opening many tabs some may be frozen due to memory pressure and won't send
        //  PAGE_LOAD_FINISHED events. Iterate over the newly opened tabs and wait for each to load.
        for (int i = 0; i < numTabs; ++i) {
            final TabModel tabModel = getActivity().getCurrentTabModel();
            final Tab tab = TabModelUtils.getTabById(tabModel, tabIds[i]);
            getInstrumentation().runOnMainSync(new Runnable() {
                @Override
                public void run() {
                    TabModelUtils.setIndex(tabModel, tabModel.indexOf(tab));
                }
            });
            try {
                pageLoadedCallbacks[i].waitForCallback(0);
            } catch (TimeoutException e) {
                fail("PAGE_LOAD_FINISHED was not received for tabId=" + tabIds[i]);
            }
        }
    }

    /**
     * Long presses the view, selects an item from the context menu, and
     * asserts that a new tab is opened and is incognito iff expectIncognito is true.
     * @param view The View to long press.
     * @param contextMenuItemId The context menu item to select on the view.
     * @param expectIncognito Whether the opened tab is expected to be incognito.
     * @param expectedUrl The expected url for the new tab.
     */
    protected void invokeContextMenuAndOpenInANewTab(View view, int contextMenuItemId,
            boolean expectIncognito, final String expectedUrl) throws InterruptedException {
        final CallbackHelper createdCallback = new CallbackHelper();
        final TabModel tabModel = getActivity().getTabModelSelector().getModel(expectIncognito);
        tabModel.addObserver(new EmptyTabModelObserver() {
            @Override
            public void didAddTab(Tab tab, TabLaunchType type) {
                if (TextUtils.equals(expectedUrl, tab.getUrl())) {
                    createdCallback.notifyCalled();
                    tabModel.removeObserver(this);
                }
            }
        });

        TestTouchUtils.longClickView(getInstrumentation(), view);
        assertTrue(getInstrumentation().invokeContextMenuAction(getActivity(),
                contextMenuItemId, 0));

        try {
            createdCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            fail("Never received tab creation event");
        }

        if (expectIncognito) {
            assertTrue(getActivity().getTabModelSelector().isIncognitoSelected());
        } else {
            assertFalse(getActivity().getTabModelSelector().isIncognitoSelected());
        }
    }
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.support.test.filters.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Tests for IncognitoTabModel.
 */
public class IncognitoTabModelTest extends ChromeTabbedActivityTestBase {
    private TabModel mTabModel;

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
        mTabModel = getActivity().getTabModelSelector().getModel(true);
    }

    private class CloseAllDuringAddTabTabModelObserver extends EmptyTabModelObserver {
        @Override
        public void willAddTab(Tab tab, TabLaunchType type) {
            mTabModel.closeAllTabs();
        }
    }

    private void createTabOnUiThread() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().getTabCreator(true).createNewTab(new LoadUrlParams("about:blank"),
                        TabLaunchType.FROM_CHROME_UI, null);
            }
        });
    }

    /**
     * Verify that a close all operation that occurs while a tab is being added does not crash the
     * browser and results in 1 valid tab. This test simulates the case where the user selects
     * "Close all incognito tabs" then quickly clicks the "+" button to add a new incognito tab.
     * See crbug.com/496651.
     */
    @SmallTest
    @Feature({"OffTheRecord"})
    @RetryOnFailure
    public void testCloseAllDuringAddTabDoesNotCrash() {
        createTabOnUiThread();
        assertEquals(1, mTabModel.getCount());
        mTabModel.addObserver(new CloseAllDuringAddTabTabModelObserver());
        createTabOnUiThread();
        assertEquals(1, mTabModel.getCount());
    }
}

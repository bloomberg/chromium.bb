// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.support.test.filters.SmallTest;
import android.test.MoreAsserts;
import android.test.UiThreadTest;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Tests for the TabModelSelectorTabObserver.
 */
public class TabModelSelectorTabObserverTest extends TabModelSelectorObserverTestBase {
    @UiThreadTest
    @SmallTest
    public void testAddingTab() {
        TestTabModelSelectorTabObserver observer = new TestTabModelSelectorTabObserver();
        Tab tab = createTestTab(false);
        assertTabDoesNotHaveObserver(tab, observer);
        mNormalTabModel.addTab(tab, 0, TabModel.TabLaunchType.FROM_LINK);
        assertTabHasObserver(tab, observer);
    }

    @UiThreadTest
    @SmallTest
    public void testClosingTab() {
        TestTabModelSelectorTabObserver observer = new TestTabModelSelectorTabObserver();
        Tab tab = createTestTab(false);
        mNormalTabModel.addTab(tab, 0, TabModel.TabLaunchType.FROM_LINK);
        assertTabHasObserver(tab, observer);
        mNormalTabModel.closeTab(tab);
        assertTabDoesNotHaveObserver(tab, observer);
    }

    @UiThreadTest
    @SmallTest
    public void testRemovingTab() {
        TestTabModelSelectorTabObserver observer = new TestTabModelSelectorTabObserver();
        Tab tab = createTestTab(false);
        mNormalTabModel.addTab(tab, 0, TabModel.TabLaunchType.FROM_LINK);
        assertTabHasObserver(tab, observer);
        mNormalTabModel.removeTab(tab);
        assertTabDoesNotHaveObserver(tab, observer);
    }

    @UiThreadTest
    @SmallTest
    public void testPreExistingTabs() {
        Tab normalTab1 = createTestTab(false);
        mNormalTabModel.addTab(normalTab1, 0, TabModel.TabLaunchType.FROM_LINK);
        Tab normalTab2 = createTestTab(false);
        mNormalTabModel.addTab(normalTab2, 1, TabModel.TabLaunchType.FROM_LINK);

        Tab incognitoTab1 = createTestTab(true);
        mIncognitoTabModel.addTab(incognitoTab1, 0, TabModel.TabLaunchType.FROM_LINK);
        Tab incognitoTab2 = createTestTab(true);
        mIncognitoTabModel.addTab(incognitoTab2, 1, TabModel.TabLaunchType.FROM_LINK);

        TestTabModelSelectorTabObserver observer = new TestTabModelSelectorTabObserver();
        assertTabHasObserver(normalTab1, observer);
        assertTabHasObserver(normalTab2, observer);
        assertTabHasObserver(incognitoTab1, observer);
        assertTabHasObserver(incognitoTab2, observer);
    }

    @UiThreadTest
    @SmallTest
    public void testDestroyRemovesObserver() {
        Tab normalTab1 = createTestTab(false);
        mNormalTabModel.addTab(normalTab1, 0, TabModel.TabLaunchType.FROM_LINK);
        Tab incognitoTab1 = createTestTab(true);
        mIncognitoTabModel.addTab(incognitoTab1, 0, TabModel.TabLaunchType.FROM_LINK);

        TestTabModelSelectorTabObserver observer = new TestTabModelSelectorTabObserver();
        assertTabHasObserver(normalTab1, observer);
        assertTabHasObserver(incognitoTab1, observer);

        observer.destroy();
        assertTabDoesNotHaveObserver(normalTab1, observer);
        assertTabDoesNotHaveObserver(incognitoTab1, observer);
    }

    @UiThreadTest
    @SmallTest
    public void testObserverAddedBeforeInitialize() {
        mSelector = new TabModelSelectorBase() {
            @Override
            public Tab openNewTab(LoadUrlParams loadUrlParams, TabLaunchType type, Tab parent,
                    boolean incognito) {
                return null;
            }
        };
        TestTabModelSelectorTabObserver observer = new TestTabModelSelectorTabObserver();
        mSelector.initialize(false, mNormalTabModel, mIncognitoTabModel);

        Tab normalTab1 = createTestTab(false);
        mNormalTabModel.addTab(normalTab1, 0, TabModel.TabLaunchType.FROM_LINK);
        assertTabHasObserver(normalTab1, observer);

        Tab incognitoTab1 = createTestTab(true);
        mIncognitoTabModel.addTab(incognitoTab1, 0, TabModel.TabLaunchType.FROM_LINK);
        assertTabHasObserver(incognitoTab1, observer);
    }

    private Tab createTestTab(boolean incognito) {
        Tab testTab = new Tab(Tab.INVALID_TAB_ID, incognito, mWindowAndroid);
        testTab.initializeNative();
        return testTab;
    }

    private class TestTabModelSelectorTabObserver extends TabModelSelectorTabObserver {
        public TestTabModelSelectorTabObserver() {
            super(mSelector);
        }
    }

    private void assertTabHasObserver(Tab tab, TabObserver observer) {
        ObserverList.RewindableIterator<TabObserver> tabObservers =
                TabTestUtils.getTabObservers(tab);
        tabObservers.rewind();
        boolean containsObserver = false;
        while (tabObservers.hasNext()) {
            if (tabObservers.next().equals(observer)) {
                containsObserver = true;
                break;
            }
        }
        assertTrue(containsObserver);
    }

    private void assertTabDoesNotHaveObserver(Tab tab, TabObserver observer) {
        ObserverList.RewindableIterator<TabObserver> tabObservers =
                TabTestUtils.getTabObservers(tab);
        tabObservers.rewind();
        while (tabObservers.hasNext()) {
            MoreAsserts.assertNotEqual(tabObservers.next(), observer);
        }
    }
}

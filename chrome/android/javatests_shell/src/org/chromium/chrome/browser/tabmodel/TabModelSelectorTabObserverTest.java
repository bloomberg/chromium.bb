// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.test.MoreAsserts;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.CommandLine;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.content.browser.test.NativeLibraryTestBase;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Tests for the TabModelSelectorTabObserver.
 */
public class TabModelSelectorTabObserverTest extends NativeLibraryTestBase {

    private TabModelSelectorBase mSelector;
    private TabModel mNormalTabModel;
    private TabModel mIncognitoTabModel;

    private WindowAndroid mWindowAndroid;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        CommandLine.init(null);
        loadNativeLibraryAndInitBrowserProcess();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                initialize();
            }
        });
    }

    private void initialize() {
        mWindowAndroid = new WindowAndroid(
                getInstrumentation().getTargetContext().getApplicationContext());

        mSelector = new TabModelSelectorBase() {
            @Override
            public Tab openNewTab(LoadUrlParams loadUrlParams, TabLaunchType type, Tab parent,
                    boolean incognito) {
                return null;
            }
        };

        TabModelOrderController orderController = new TabModelOrderController(mSelector);
        TabModelDelegate delegate = new TabModelDelegate() {
            @Override
            public void selectModel(boolean incognito) {
                mSelector.selectModel(incognito);
            }

            @Override
            public void requestToShowTab(Tab tab, TabSelectionType type) {
            }

            @Override
            public boolean isSessionRestoreInProgress() {
                return false;
            }

            @Override
            public boolean isInOverviewMode() {
                return false;
            }

            @Override
            public TabModel getModel(boolean incognito) {
                return mSelector.getModel(incognito);
            }

            @Override
            public TabModel getCurrentModel() {
                return mSelector.getCurrentModel();
            }

            @Override
            public boolean closeAllTabsRequest(boolean incognito) {
                return false;
            }
        };
        mNormalTabModel = new TabModelBase(false, orderController, delegate) {
            @Override
            protected Tab createTabWithWebContents(boolean incognito, WebContents webContents,
                    int parentId) {
                return null;
            }

            @Override
            protected Tab createNewTabForDevTools(String url) {
                return null;
            }
        };

        mIncognitoTabModel = new TabModelBase(true, orderController, delegate) {
            @Override
            protected Tab createTabWithWebContents(boolean incognito, WebContents webContents,
                    int parentId) {
                return null;
            }

            @Override
            protected Tab createNewTabForDevTools(String url) {
                return null;
            }
        };

        mSelector.initialize(false, mNormalTabModel, mIncognitoTabModel);
    }

    @UiThreadTest
    @SmallTest
    public void testAddingTab() {
        TestTabModelSelectorTabObserver observer = new TestTabModelSelectorTabObserver();
        TestTab tab = new TestTab(false);
        assertTabDoesNotHaveObserver(tab, observer);
        mNormalTabModel.addTab(tab, 0, TabModel.TabLaunchType.FROM_LINK);
        assertTabHasObserver(tab, observer);
    }

    @UiThreadTest
    @SmallTest
    public void testRemovingTab() {
        TestTabModelSelectorTabObserver observer = new TestTabModelSelectorTabObserver();
        TestTab tab = new TestTab(false);
        mNormalTabModel.addTab(tab, 0, TabModel.TabLaunchType.FROM_LINK);
        assertTabHasObserver(tab, observer);
        mNormalTabModel.closeTab(tab);
        assertTabDoesNotHaveObserver(tab, observer);
    }

    @UiThreadTest
    @SmallTest
    public void testPreExistingTabs() {
        TestTab normalTab1 = new TestTab(false);
        mNormalTabModel.addTab(normalTab1, 0, TabModel.TabLaunchType.FROM_LINK);
        TestTab normalTab2 = new TestTab(false);
        mNormalTabModel.addTab(normalTab2, 1, TabModel.TabLaunchType.FROM_LINK);

        TestTab incognitoTab1 = new TestTab(true);
        mIncognitoTabModel.addTab(incognitoTab1, 0, TabModel.TabLaunchType.FROM_LINK);
        TestTab incognitoTab2 = new TestTab(true);
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
        TestTab normalTab1 = new TestTab(false);
        mNormalTabModel.addTab(normalTab1, 0, TabModel.TabLaunchType.FROM_LINK);
        TestTab incognitoTab1 = new TestTab(true);
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

        TestTab normalTab1 = new TestTab(false);
        mNormalTabModel.addTab(normalTab1, 0, TabModel.TabLaunchType.FROM_LINK);
        assertTabHasObserver(normalTab1, observer);

        TestTab incognitoTab1 = new TestTab(true);
        mIncognitoTabModel.addTab(incognitoTab1, 0, TabModel.TabLaunchType.FROM_LINK);
        assertTabHasObserver(incognitoTab1, observer);
    }

    private class TestTab extends Tab {
        public TestTab(boolean incognito) {
            super(incognito, null, mWindowAndroid);
            initializeNative();
        }

        // Exists to expose the method to the test.
        @Override
        protected ObserverList.RewindableIterator<TabObserver> getTabObservers() {
            return super.getTabObservers();
        }
    }

    private class TestTabModelSelectorTabObserver extends TabModelSelectorTabObserver {
        public TestTabModelSelectorTabObserver() {
            super(mSelector);
        }
    }

    private void assertTabHasObserver(TestTab tab, TabObserver observer) {
        ObserverList.RewindableIterator<TabObserver> tabObservers = tab.getTabObservers();
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

    private void assertTabDoesNotHaveObserver(TestTab tab, TabObserver observer) {
        ObserverList.RewindableIterator<TabObserver> tabObservers = tab.getTabObservers();
        tabObservers.rewind();
        while (tabObservers.hasNext()) {
            MoreAsserts.assertNotEqual(tabObservers.next(), observer);
        }
    }
}

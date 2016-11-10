// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.CommandLine;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.content.browser.test.NativeLibraryTestBase;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;

import java.util.HashSet;
import java.util.Set;

/**
 * Basis for testing tab model selector observers.
 */
public class TabModelSelectorObserverTestBase extends NativeLibraryTestBase {
    protected TabModelSelectorBase mSelector;
    protected TabModelSelectorTestTabModel mNormalTabModel;
    protected TabModelSelectorTestTabModel mIncognitoTabModel;

    protected WindowAndroid mWindowAndroid;

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
        TabContentManager tabContentManager =
                new TabContentManager(getInstrumentation().getTargetContext(), null, false);
        TabPersistencePolicy persistencePolicy = new TabbedModeTabPersistencePolicy(0, false);
        TabPersistentStore tabPersistentStore = new TabPersistentStore(persistencePolicy, mSelector,
                null, null);

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
        mNormalTabModel = new TabModelSelectorTestTabModel(
                false, orderController, tabContentManager, tabPersistentStore, delegate);

        mIncognitoTabModel = new TabModelSelectorTestTabModel(
                true, orderController, tabContentManager, tabPersistentStore, delegate);

        mSelector.initialize(false, mNormalTabModel, mIncognitoTabModel);
    }

    /**
     * Test TabModel that exposes the needed capabilities for testing.
     */
    public static class TabModelSelectorTestTabModel extends TabModelImpl {
        private Set<TabModelObserver> mObserverSet = new HashSet<>();

        public TabModelSelectorTestTabModel(
                boolean incognito, TabModelOrderController orderController,
                TabContentManager tabContentManager, TabPersistentStore tabPersistentStore,
                TabModelDelegate modelDelegate) {
            super(incognito, false, null, null, null, orderController, tabContentManager,
                    tabPersistentStore, modelDelegate, false);
        }

        @Override
        public void addObserver(TabModelObserver observer) {
            super.addObserver(observer);
            mObserverSet.add(observer);
        }

        @Override
        public void removeObserver(TabModelObserver observer) {
            super.removeObserver(observer);
            mObserverSet.remove(observer);
        }

        public Set<TabModelObserver> getObservers() {
            return mObserverSet;
        }
    }
}

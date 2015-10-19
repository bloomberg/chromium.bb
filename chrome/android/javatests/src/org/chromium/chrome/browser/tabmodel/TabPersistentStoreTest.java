// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.content.Context;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.SparseArray;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;
import org.chromium.chrome.browser.tabmodel.TestTabModelDirectory.TabModelMetaDataInfo;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.content.browser.test.NativeLibraryTestBase;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;

/** Tests for the TabPersistentStore. */
public class TabPersistentStoreTest extends NativeLibraryTestBase {
    private static final int SELECTOR_INDEX = 0;

    private static class TabRestoredDetails {
        public int index;
        public int id;
        public String url;
        public boolean isStandardActiveIndex;
        public boolean isIncognitoActiveIndex;

        /** Store information about a Tab that's been restored. */
        TabRestoredDetails(int index, int id, String url,
                boolean isStandardActiveIndex, boolean isIncognitoActiveIndex) {
            this.index = index;
            this.id = id;
            this.url = url;
            this.isStandardActiveIndex = isStandardActiveIndex;
            this.isIncognitoActiveIndex = isIncognitoActiveIndex;
        }
    }

    private static class MockTabCreator extends TabCreator {
        public final SparseArray<TabState> created = new SparseArray<TabState>();
        public final CallbackHelper callback = new CallbackHelper();
        public int idOfFirstCreatedTab = Tab.INVALID_TAB_ID;

        @Override
        public boolean createsTabsAsynchronously() {
            return false;
        }

        @Override
        public Tab createNewTab(
                LoadUrlParams loadUrlParams, TabModel.TabLaunchType type, Tab parent) {
            int id = TabIdManager.getInstance().generateValidId(Tab.INVALID_TAB_ID);
            storeTabInfo(null, id);
            return null;
        }

        @Override
        public Tab createFrozenTab(TabState state, int id, int index) {
            storeTabInfo(state, id);
            return null;
        }

        @Override
        public boolean createTabWithWebContents(
                WebContents webContents, int parentId, TabLaunchType type, String url) {
            return false;
        }

        @Override
        public Tab launchUrl(String url, TabModel.TabLaunchType type) {
            return null;
        }

        private void storeTabInfo(TabState state, int id) {
            if (created.size() == 0) idOfFirstCreatedTab = id;
            created.put(id, state);
            callback.notifyCalled();
        }
    }

    private static class MockTabCreatorManager implements TabCreatorManager {
        public final MockTabCreator regularCreator = new MockTabCreator();
        public final MockTabCreator incognitoCreator = new MockTabCreator();

        @Override
        public TabCreator getTabCreator(boolean incognito) {
            return incognito ? incognitoCreator : regularCreator;
        }
    }

    private static class MockTabPersistentStoreObserver implements TabPersistentStoreObserver {
        public final CallbackHelper initializedCallback = new CallbackHelper();
        public final CallbackHelper detailsReadCallback = new CallbackHelper();
        public final CallbackHelper stateLoadedCallback = new CallbackHelper();
        public final ArrayList<TabRestoredDetails> mDetails = new ArrayList<TabRestoredDetails>();

        public int mTabCountAtStartup = -1;

        @Override
        public void onInitialized(int tabCountAtStartup) {
            mTabCountAtStartup = tabCountAtStartup;
            initializedCallback.notifyCalled();
        }

        @Override
        public void onDetailsRead(int index, int id, String url,
                boolean isStandardActiveIndex, boolean isIncognitoActiveIndex) {
            mDetails.add(new TabRestoredDetails(
                    index, id, url, isStandardActiveIndex, isIncognitoActiveIndex));
            detailsReadCallback.notifyCalled();
        }

        @Override
        public void onStateLoaded(Context context) {
            stateLoadedCallback.notifyCalled();
        }
    }

    /** Class for mocking out the directory containing all of the TabState files. */
    private TestTabModelDirectory mMockDirectory;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        loadNativeLibraryAndInitBrowserProcess();
        mMockDirectory = new TestTabModelDirectory(getInstrumentation().getTargetContext(),
                "TabPersistentStoreTest", Integer.toString(SELECTOR_INDEX));
    }

    @Override
    public void tearDown() throws Exception {
        mMockDirectory.tearDown();
        super.tearDown();
    }

    @SmallTest
    public void testBasic() throws Exception {
        mMockDirectory.writeTabModelFiles(TestTabModelDirectory.TAB_MODEL_METADATA_V4, true);

        // Set up the TabPersistentStore.
        Context context = getInstrumentation().getTargetContext();
        TabPersistentStore.setBaseStateDirectory(mMockDirectory.getBaseDirectory());
        int numExpectedTabs = TestTabModelDirectory.TAB_MODEL_METADATA_V4.contents.length;

        MockTabModelSelector mockSelector = new MockTabModelSelector(0, 0, null);
        MockTabCreatorManager mockManager = new MockTabCreatorManager();
        MockTabCreator regularCreator = mockManager.regularCreator;
        MockTabPersistentStoreObserver mockObserver = new MockTabPersistentStoreObserver();
        TabPersistentStore store =
                new TabPersistentStore(mockSelector, 0, context, mockManager, mockObserver);

        // Make sure the metadata file loads properly and in order.
        store.loadState();
        mockObserver.initializedCallback.waitForCallback(0, 1);
        assertEquals(numExpectedTabs, mockObserver.mTabCountAtStartup);

        mockObserver.detailsReadCallback.waitForCallback(0, numExpectedTabs);
        assertEquals(numExpectedTabs, mockObserver.mDetails.size());
        for (int i = 0; i < numExpectedTabs; i++) {
            TabRestoredDetails details = mockObserver.mDetails.get(i);
            assertEquals(i, details.index);
            assertEquals(TestTabModelDirectory.TAB_MODEL_METADATA_V4.contents[i].tabId, details.id);
            assertEquals(TestTabModelDirectory.TAB_MODEL_METADATA_V4.contents[i].url, details.url);
            assertEquals(details.id == TestTabModelDirectory.TAB_MODEL_METADATA_V4.selectedTabId,
                    details.isStandardActiveIndex);
            assertEquals(false, details.isIncognitoActiveIndex);
        }

        // Restore the TabStates.  The first Tab added should be the most recently selected tab.
        store.restoreTabs(true);
        regularCreator.callback.waitForCallback(0, 1);
        assertEquals(TestTabModelDirectory.TAB_MODEL_METADATA_V4.selectedTabId,
                regularCreator.idOfFirstCreatedTab);

        // Confirm that all the TabStates were read from storage (i.e. non-null).
        mockObserver.stateLoadedCallback.waitForCallback(0, 1);
        for (int i = 0; i < TestTabModelDirectory.TAB_MODEL_METADATA_V4.contents.length; i++) {
            int tabId = TestTabModelDirectory.TAB_MODEL_METADATA_V4.contents[i].tabId;
            assertNotNull(regularCreator.created.get(tabId));
        }
    }

    @SmallTest
    public void testInterruptedButStillRestoresAllTabs() throws Exception {
        mMockDirectory.writeTabModelFiles(TestTabModelDirectory.TAB_MODEL_METADATA_V4, true);

        Context context = getInstrumentation().getTargetContext();
        TabPersistentStore.setBaseStateDirectory(mMockDirectory.getBaseDirectory());
        int numExpectedTabs = TestTabModelDirectory.TAB_MODEL_METADATA_V4.contents.length;

        // Load up one TabPersistentStore, but don't load up the TabState files.  This prevents the
        // Tabs from being added to the TabModel.
        MockTabModelSelector firstSelector = new MockTabModelSelector(0, 0, null);
        MockTabCreatorManager firstManager = new MockTabCreatorManager();
        MockTabPersistentStoreObserver firstObserver = new MockTabPersistentStoreObserver();
        final TabPersistentStore firstStore = new TabPersistentStore(
                firstSelector, 0, context, firstManager, firstObserver);
        firstStore.loadState();
        firstObserver.initializedCallback.waitForCallback(0, 1);
        assertEquals(numExpectedTabs, firstObserver.mTabCountAtStartup);
        firstObserver.detailsReadCallback.waitForCallback(0, numExpectedTabs);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                firstStore.saveState();
            }
        });

        // Prepare a second TabPersistentStore.
        MockTabModelSelector secondSelector = new MockTabModelSelector(0, 0, null);
        MockTabCreatorManager secondManager = new MockTabCreatorManager();
        MockTabCreator secondCreator = secondManager.regularCreator;
        MockTabPersistentStoreObserver secondObserver = new MockTabPersistentStoreObserver();
        TabPersistentStore secondStore = new TabPersistentStore(
                secondSelector, 0, context, secondManager, secondObserver);

        // The second TabPersistentStore reads the file written by the first TabPersistentStore.
        // Make sure that all of the Tabs appear in the new one -- even though the new file was
        // written before the first TabPersistentStore loaded any TabState files and added them to
        // the TabModels.
        secondStore.loadState();
        secondObserver.initializedCallback.waitForCallback(0, 1);
        assertEquals(numExpectedTabs, secondObserver.mTabCountAtStartup);

        secondObserver.detailsReadCallback.waitForCallback(0, numExpectedTabs);
        assertEquals(numExpectedTabs, secondObserver.mDetails.size());
        for (int i = 0; i < numExpectedTabs; i++) {
            TabRestoredDetails details = secondObserver.mDetails.get(i);

            // Find the details for the current Tab ID.
            // TODO(dfalcantara): Revisit this bit when tab ordering is correctly preserved.
            TestTabModelDirectory.TabStateInfo currentInfo = null;
            for (int j = 0; j < numExpectedTabs && currentInfo == null; j++) {
                if (TestTabModelDirectory.TAB_MODEL_METADATA_V4.contents[j].tabId == details.id) {
                    currentInfo = TestTabModelDirectory.TAB_MODEL_METADATA_V4.contents[j];
                }
            }

            // TODO(dfalcantara): This won't be properly set until we have tab ordering preserved.
            // assertEquals(details.id == TestTabModelDirectory.TAB_MODEL_METADATA_V4_SELECTED_ID,
            //        details.isStandardActiveIndex);

            assertEquals(currentInfo.url, details.url);
            assertEquals(false, details.isIncognitoActiveIndex);
        }

        // Restore all of the TabStates.  Confirm that all the TabStates were read (i.e. non-null).
        secondStore.restoreTabs(true);
        secondObserver.stateLoadedCallback.waitForCallback(0, 1);
        for (int i = 0; i < numExpectedTabs; i++) {
            int tabId = TestTabModelDirectory.TAB_MODEL_METADATA_V4.contents[i].tabId;
            assertNotNull(secondCreator.created.get(tabId));
        }
    }

    @SmallTest
    public void testMissingTabStateButStillRestoresTab() throws Exception {
        TabModelMetaDataInfo info = TestTabModelDirectory.TAB_MODEL_METADATA_V5;

        // Write out info for all but the third tab (arbitrarily chosen).
        mMockDirectory.writeTabModelFiles(info, false);
        for (int i = 0; i < info.contents.length; i++) {
            if (i != 2) mMockDirectory.writeTabStateFile(info.contents[i]);
        }

        // Set up the TabPersistentStore.
        Context context = getInstrumentation().getTargetContext();
        TabPersistentStore.setBaseStateDirectory(mMockDirectory.getBaseDirectory());
        int numExpectedTabs = info.contents.length;

        MockTabModelSelector mockSelector = new MockTabModelSelector(0, 0, null);
        MockTabCreatorManager mockManager = new MockTabCreatorManager();
        MockTabPersistentStoreObserver mockObserver = new MockTabPersistentStoreObserver();
        TabPersistentStore store =
                new TabPersistentStore(mockSelector, 0, context, mockManager, mockObserver);

        // Make sure the metadata file loads properly and in order.
        store.loadState();
        mockObserver.initializedCallback.waitForCallback(0, 1);
        assertEquals(numExpectedTabs, mockObserver.mTabCountAtStartup);

        mockObserver.detailsReadCallback.waitForCallback(0, numExpectedTabs);
        assertEquals(numExpectedTabs, mockObserver.mDetails.size());
        for (int i = 0; i < numExpectedTabs; i++) {
            TabRestoredDetails details = mockObserver.mDetails.get(i);
            assertEquals(i, details.index);
            assertEquals(info.contents[i].tabId, details.id);
            assertEquals(info.contents[i].url, details.url);
            assertEquals(details.id == info.selectedTabId, details.isStandardActiveIndex);
            assertEquals(false, details.isIncognitoActiveIndex);
        }

        // Restore the TabStates, and confirm that the correct number of tabs is created even with
        // one missing.
        store.restoreTabs(true);
        mockObserver.stateLoadedCallback.waitForCallback(0, 1);
        assertEquals(info.contents.length, mockManager.regularCreator.created.size());
    }

    @SmallTest
    public void testRestoresTabWithMissingTabStateWhileIgnoringIncognitoTab() throws Exception {
        TabModelMetaDataInfo info = TestTabModelDirectory.TAB_MODEL_METADATA_V5_WITH_INCOGNITO;

        // Write out info for all but the third tab (arbitrarily chosen).
        mMockDirectory.writeTabModelFiles(info, false);
        for (int i = 0; i < info.contents.length; i++) {
            if (i != 2) mMockDirectory.writeTabStateFile(info.contents[i]);
        }

        // Set up the TabPersistentStore.
        Context context = getInstrumentation().getTargetContext();
        TabPersistentStore.setBaseStateDirectory(mMockDirectory.getBaseDirectory());
        int numExpectedTabs = info.contents.length;

        MockTabModelSelector mockSelector = new MockTabModelSelector(0, 0, null);
        MockTabCreatorManager mockManager = new MockTabCreatorManager();
        MockTabPersistentStoreObserver mockObserver = new MockTabPersistentStoreObserver();
        TabPersistentStore store =
                new TabPersistentStore(mockSelector, 0, context, mockManager, mockObserver);

        // Load the TabModel metadata.
        store.loadState();
        mockObserver.initializedCallback.waitForCallback(0, 1);
        assertEquals(numExpectedTabs, mockObserver.mTabCountAtStartup);
        mockObserver.detailsReadCallback.waitForCallback(0, numExpectedTabs);
        assertEquals(numExpectedTabs, mockObserver.mDetails.size());

        // TODO(dfalcantara): Expand MockTabModel* to support frozen and Incognito Tabs, then change
        //                    the MockTabCreators so they actually create Tabs.

        // Restore the TabStates, and confirm that the correct number of tabs is created even with
        // one missing.  No Incognito tabs should be created because the TabState is missing.
        store.restoreTabs(true);
        mockObserver.stateLoadedCallback.waitForCallback(0, 1);
        assertEquals(info.getNumRegularTabs(), mockManager.regularCreator.created.size());
        assertEquals(0, mockManager.incognitoCreator.created.size());
    }
}

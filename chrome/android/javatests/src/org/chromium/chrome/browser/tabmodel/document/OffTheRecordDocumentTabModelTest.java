// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel.document;

import android.content.Context;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.CommandLine;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.OffTheRecordTabModel.OffTheRecordTabModelDelegate;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.util.browser.tabmodel.document.MockActivityDelegate;
import org.chromium.chrome.test.util.browser.tabmodel.document.MockDocumentTabCreatorManager;
import org.chromium.chrome.test.util.browser.tabmodel.document.MockStorageDelegate;
import org.chromium.chrome.test.util.browser.tabmodel.document.TestInitializationObserver;
import org.chromium.content.browser.test.NativeLibraryTestBase;

/**
 * Tests the functionality of the OffTheRecordDocumentTabModel.
 * These tests rely on the DocumentTabModelImpl adding tasks it finds via the ActivityManager to its
 * internal list, instead of loading a task file containing an explicit list like the regular
 * DocumentTabModelTests do.
 */
public class OffTheRecordDocumentTabModelTest extends NativeLibraryTestBase {
    private MockActivityDelegate mActivityDelegate;
    private MockStorageDelegate mStorageDelegate;
    private MockDocumentTabCreatorManager mTabCreatorManager;

    private OffTheRecordDocumentTabModel mTabModel;
    private Profile mProfile;
    private Context mContext;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        CommandLine.init(null);
        loadNativeLibraryAndInitBrowserProcess();

        mContext = getInstrumentation().getTargetContext();
        mStorageDelegate = new MockStorageDelegate(mContext.getCacheDir());
        mActivityDelegate = new MockActivityDelegate();
        mTabCreatorManager = new MockDocumentTabCreatorManager();
    }

    private void createTabModel() {
        final OffTheRecordTabModelDelegate delegate = new OffTheRecordTabModelDelegate() {
            private DocumentTabModel mModel;

            @Override
            public TabModel createTabModel() {
                mModel = new DocumentTabModelImpl(mActivityDelegate, mStorageDelegate,
                        mTabCreatorManager, true, Tab.INVALID_TAB_ID, mContext);
                return mModel;
            }

            @Override
            public boolean doOffTheRecordTabsExist() {
                return mModel == null ? false : (mModel.getCount() > 0);
            }

        };
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mTabModel = new OffTheRecordDocumentTabModel(delegate, mActivityDelegate);
            }
        });
    }

    private void initializeTabModel() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mTabModel.startTabStateLoad();
                mTabModel.initializeNative();
                mProfile = mTabModel.getProfile();
                assertTrue(mProfile.isNativeInitialized());
            }
        });
        TestInitializationObserver.waitUntilState(
                mTabModel, DocumentTabModelImpl.STATE_DESERIALIZE_END);
    }

    @SmallTest
    public void testNotCreatedWithoutIncognitoTask() throws Exception {
        mActivityDelegate.addTask(false, 11684, "http://regular.tab");
        createTabModel();
        assertEquals(0, mTabModel.getCount());
        assertFalse(mTabModel.isDocumentTabModelImplCreated());
    }

    @SmallTest
    public void testCreatedIfIncognitoTaskExists() throws Exception {
        mActivityDelegate.addTask(true, 11684, "http://incognito.tab");
        createTabModel();
        assertEquals(1, mTabModel.getCount());
        assertTrue(mTabModel.isDocumentTabModelImplCreated());
    }

    @SmallTest
    public void testTabModelDestructionWhenSwipedAway() throws Exception {
        mActivityDelegate.addTask(false, 11683, "http://regular.tab");
        mActivityDelegate.addTask(true, 11684, "http://incognito.tab");
        mActivityDelegate.addTask(true, 11685, "http://second.incognito.tab");
        createTabModel();
        assertEquals(2, mTabModel.getCount());
        assertTrue(mTabModel.isDocumentTabModelImplCreated());
        initializeTabModel();

        mActivityDelegate.removeTask(true, 11684);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mTabModel.updateRecentlyClosed();
            }
        });
        assertEquals(1, mTabModel.getCount());
        assertTrue(mTabModel.isDocumentTabModelImplCreated());
        assertTrue(mProfile.isNativeInitialized());

        mActivityDelegate.removeTask(true, 11685);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mTabModel.updateRecentlyClosed();
            }
        });
        assertEquals(0, mTabModel.getCount());
        assertFalse(mTabModel.isDocumentTabModelImplCreated());
        assertFalse(mProfile.isNativeInitialized());
    }

    @SmallTest
    public void testTabModelDestructionWhenNativeNotInitialized() throws Exception {
        mActivityDelegate.addTask(false, 11683, "http://regular.tab");
        mActivityDelegate.addTask(true, 11684, "http://incognito.tab");
        mActivityDelegate.addTask(true, 11685, "http://second.incognito.tab");
        createTabModel();
        assertEquals(2, mTabModel.getCount());
        assertTrue(mTabModel.isDocumentTabModelImplCreated());

        mActivityDelegate.removeTask(true, 11684);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mTabModel.updateRecentlyClosed();
            }
        });
        assertEquals(1, mTabModel.getCount());
        assertTrue(mTabModel.isDocumentTabModelImplCreated());

        mActivityDelegate.removeTask(true, 11685);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mTabModel.updateRecentlyClosed();
            }
        });
        assertEquals(0, mTabModel.getCount());
        assertFalse(mTabModel.isDocumentTabModelImplCreated());
    }

    @SmallTest
    public void testTabModelDestructionAfterCloseTabAt() throws Exception {
        mActivityDelegate.addTask(false, 11683, "http://regular.tab");
        mActivityDelegate.addTask(true, 11684, "http://incognito.tab");
        mActivityDelegate.addTask(true, 11685, "http://second.incognito.tab");
        createTabModel();
        assertEquals(2, mTabModel.getCount());
        assertTrue(mTabModel.isDocumentTabModelImplCreated());
        initializeTabModel();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mTabModel.closeTabAt(0);
            }
        });
        assertEquals(1, mActivityDelegate.getTasksFromRecents(false).size());
        assertEquals(1, mActivityDelegate.getTasksFromRecents(true).size());
        assertEquals(1, mTabModel.getCount());
        assertTrue(mTabModel.isDocumentTabModelImplCreated());
        assertTrue(mProfile.isNativeInitialized());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mTabModel.closeTabAt(0);
            }
        });
        assertEquals(1, mActivityDelegate.getTasksFromRecents(false).size());
        assertEquals(0, mActivityDelegate.getTasksFromRecents(true).size());
        assertEquals(0, mTabModel.getCount());
        assertFalse(mTabModel.isDocumentTabModelImplCreated());
        assertFalse(mProfile.isNativeInitialized());
    }

    @SmallTest
    public void testTabModelDestructionAfterCloseAllTabs() throws Exception {
        mActivityDelegate.addTask(false, 11683, "http://regular.tab");
        mActivityDelegate.addTask(true, 11684, "http://incognito.tab");
        mActivityDelegate.addTask(true, 11685, "http://second.incognito.tab");
        createTabModel();
        assertEquals(2, mTabModel.getCount());
        assertTrue(mTabModel.isDocumentTabModelImplCreated());
        initializeTabModel();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mTabModel.closeAllTabs();
            }
        });
        assertEquals(1, mActivityDelegate.getTasksFromRecents(false).size());
        assertEquals(0, mActivityDelegate.getTasksFromRecents(true).size());
        assertEquals(0, mTabModel.getCount());
        assertFalse(mTabModel.isDocumentTabModelImplCreated());
        assertFalse(mProfile.isNativeInitialized());
    }
}

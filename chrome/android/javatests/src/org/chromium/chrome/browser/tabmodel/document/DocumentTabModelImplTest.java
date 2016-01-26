// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel.document;

import android.content.Intent;
import android.net.Uri;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.CommandLine;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.util.browser.tabmodel.document.MockActivityDelegate;
import org.chromium.chrome.test.util.browser.tabmodel.document.MockDocumentTabCreatorManager;
import org.chromium.chrome.test.util.browser.tabmodel.document.MockStorageDelegate;
import org.chromium.chrome.test.util.browser.tabmodel.document.TestInitializationObserver;
import org.chromium.content.browser.test.NativeLibraryTestBase;

import java.util.HashMap;
import java.util.Map;

/**
 * Tests the functionality of the DocumentTabModel.
 */
public class DocumentTabModelImplTest extends NativeLibraryTestBase {
    private static final String MODEL_STATE_WITH_1010_1011 = "CgUgACjyBwoFIAEo8wc=";

    private static final String TAB_STATE_1010_ERFWORLD_RETARGETABLE =
            "AAABSVhnsswAAAFkYAEAAAAAAAABAAAAAAAAAFABAABMAQAAAAAAACcAAABodHRwOi8vd3d3LmVyZndvcmxkLm"
            + "NvbS9lcmZfc3RyZWFtL3ZpZXcAAAAAAMQAAADAAAAAFQAAAAAAAABOAAAAaAB0AHQAcAA6AC8ALwB3AHcAdw"
            + "AuAGUAcgBmAHcAbwByAGwAZAAuAGMAbwBtAC8AZQByAGYAXwBzAHQAcgBlAGEAbQAvAHYAaQBlAHcAAAD///"
            + "//AAAAAAAAAAD/////AAAAAAgAAAAAAAAAAAAAAM2oGVWBBgUAzqgZVYEGBQDPqBlVgQYFAAEAAAAIAAAAAA"
            + "AAAAAAAAAIAAAAAAAAAAAAAAAAAAAAAAAAAP////8AAAAAAAAACAAAAAAAAAAAAQAAACcAAABodHRwOi8vd3"
            + "d3LmVyZndvcmxkLmNvbS9lcmZfc3RyZWFtL3ZpZXcAAAAAAOrxn50XZS4AAAAAAMgAAAD/////AAAAAAACAA"
            + "AAAAAAAAAA";

    private static final String TAB_STATE_1011_REDDIT =
            "AAABSVhw+HkAAAJkYAIAAAAAAAACAAAAAQAAACABAAAcAQAAAAAAABcAAABjaHJvbWUtbmF0aXZlOi8vbmV3"
            + "dGFiLwAHAAAATgBlAHcAIAB0AGEAYgAAAKQAAACgAAAAFQAAAAAAAAAuAAAAYwBoAHIAbwBtAGUALQBuAGEA"
            + "dABpAHYAZQA6AC8ALwBuAGUAdwB0AGEAYgAvAAAA/////wAAAAAAAAAA/////wAAAAAIAAAAAAAAAAAA8D9M"
            + "Bk15gQYFAE0GTXmBBgUATgZNeYEGBQABAAAACAAAAAAAAAAAAPC/CAAAAAAAAAAAAPC/AAAAAAAAAAD/////"
            + "AAAAAAYAAAAAAAAAAAAAAAEAAAAXAAAAY2hyb21lLW5hdGl2ZTovL25ld3RhYi8AAAAAAN5f08EXZS4AAAAA"
            + "AAAAAAAsAQAAKAEAAAEAAAAfAAAAaHR0cDovL3d3dy5yZWRkaXQuY29tL3IvYW5kcm9pZAAAAAAAtAAAALAA"
            + "AAAVAAAAAAAAAD4AAABoAHQAdABwADoALwAvAHcAdwB3AC4AcgBlAGQAZABpAHQALgBjAG8AbQAvAHIALwBh"
            + "AG4AZAByAG8AaQBkAAAA/////wAAAAAAAAAA/////wAAAAAIAAAAAAAAAAAAAABPBk15gQYFAFAGTXmBBgUA"
            + "TgZNeYEGBQABAAAACAAAAAAAAAAAAAAACAAAAAAAAAAAAAAAAAAAAAAAAAD/////AAAAAAEAAAIAAAAAAAAA"
            + "AAEAAAAbAAAAaHR0cDovL3JlZGRpdC5jb20vci9hbmRyb2lkAAAAAAAB65zCF2UuAAAAAADIAAAA/////wAA"
            + "AAAAAgAAAAAAAAAAAA==";

    private static class CloseRunnable implements Runnable {
        final DocumentTabModel mTabModel;
        final int mIndex;
        boolean mSucceeded;

        public CloseRunnable(DocumentTabModel model, int index) {
            mTabModel = model;
            mIndex = index;
        }

        @Override
        public void run() {
            mSucceeded = mTabModel.closeTabAt(mIndex);
        }

        static boolean closeTabAt(DocumentTabModel model, int index) throws Exception {
            CloseRunnable runnable = new CloseRunnable(model, index);
            ThreadUtils.runOnUiThreadBlocking(runnable);
            return runnable.mSucceeded;
        }
    }

    private MockActivityDelegate mActivityDelegate;
    private MockStorageDelegate mStorageDelegate;
    private MockDocumentTabCreatorManager mTabCreatorManager;
    private DocumentTabModel mTabModel;
    private AdvancedMockContext mContext;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        RecordHistogram.disableForTests();
        CommandLine.init(null);
        loadNativeLibraryAndInitBrowserProcess();

        mActivityDelegate = new MockActivityDelegate();
        mTabCreatorManager = new MockDocumentTabCreatorManager();
        mContext = new AdvancedMockContext(getInstrumentation().getTargetContext());
        mStorageDelegate = new MockStorageDelegate(mContext.getCacheDir());
    }

    @Override
    protected void tearDown() throws Exception {
        mStorageDelegate.ensureDirectoryDestroyed();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                if (mTabModel.isNativeInitialized()) mTabModel.destroy();
            }
        });

        super.tearDown();
    }

    private void setupDocumentTabModel() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mTabModel = new DocumentTabModelImpl(mActivityDelegate, mStorageDelegate,
                        mTabCreatorManager, false, 1010, mContext);
                mTabModel.startTabStateLoad();
            }
        });
    }

    private void initializeNativeTabModel() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mTabModel.initializeNative();
            }
        });
        TestInitializationObserver.waitUntilState(
                mTabModel, DocumentTabModelImpl.STATE_DESERIALIZE_END);
    }

    @SmallTest
    public void testBasic() throws Exception {
        mActivityDelegate.addTask(false, 1010, "http://erfworld.com");
        mActivityDelegate.addTask(false, 1011, "http://reddit.com/r/android");

        mStorageDelegate.setTaskFileBytesFromEncodedString(MODEL_STATE_WITH_1010_1011);
        mStorageDelegate.addEncodedTabState(1010, false, TAB_STATE_1010_ERFWORLD_RETARGETABLE);
        mStorageDelegate.addEncodedTabState(1011, false, TAB_STATE_1011_REDDIT);

        setupDocumentTabModel();

        // Confirm the data from the task file is restored correctly.
        assertEquals(2, mTabModel.getCount());
        assertEquals(1010, mTabModel.getTabAt(0).getId());
        assertEquals(1011, mTabModel.getTabAt(1).getId());
        assertEquals("http://erfworld.com", mTabModel.getInitialUrlForDocument(1010));
        assertEquals("http://reddit.com/r/android", mTabModel.getInitialUrlForDocument(1011));
        // Due to using AsyncTask to fetch metadata, at this point both should be non-retargetable
        // by default until that AsyncTask completes.
        assertEquals(false, mTabModel.isRetargetable(1010));
        assertEquals(false, mTabModel.isRetargetable(1011));

        // State of the tabs.
        assertTrue(mTabModel.isTabStateReady(1010));
        assertNotNull(mTabModel.getTabStateForDocument(1010));
        assertNull(mTabModel.getCurrentUrlForDocument(1010));
        assertNull(mTabModel.getCurrentUrlForDocument(1011));

        // Wait until the tab states are loaded.
        TestInitializationObserver.waitUntilState(
                mTabModel, DocumentTabModelImpl.STATE_LOAD_TAB_STATE_BG_END);
        assertNotNull(mTabModel.getTabStateForDocument(1011));
        // Startup AsyncTasks must be complete by now since they are both on serial executor.
        assertEquals(true, mTabModel.isRetargetable(1010));
        assertEquals(false, mTabModel.isRetargetable(1011));

        // Load the native library, wait until the states are deserialized, then check their values.
        initializeNativeTabModel();
        assertEquals("http://www.erfworld.com/erf_stream/view",
                mTabModel.getCurrentUrlForDocument(1010));
        assertEquals("http://www.reddit.com/r/android", mTabModel.getCurrentUrlForDocument(1011));
    }

    @SmallTest
    public void testIncognitoIgnored() throws Exception {
        mActivityDelegate.addTask(false, 1010, "http://erfworld.com");
        mActivityDelegate.addTask(false, 1011, "http://reddit.com/r/android");
        mActivityDelegate.addTask(true, 1012, "http://incognito.com/ignored");

        mStorageDelegate.setTaskFileBytesFromEncodedString(MODEL_STATE_WITH_1010_1011);
        mStorageDelegate.addEncodedTabState(1010, false, TAB_STATE_1010_ERFWORLD_RETARGETABLE);
        mStorageDelegate.addEncodedTabState(1011, false, TAB_STATE_1011_REDDIT);

        setupDocumentTabModel();

        // Confirm the data from the task file is restored correctly.
        assertEquals(2, mTabModel.getCount());
        assertEquals(1010, mTabModel.getTabAt(0).getId());
        assertEquals(1011, mTabModel.getTabAt(1).getId());
        assertEquals("http://erfworld.com", mTabModel.getInitialUrlForDocument(1010));
        assertEquals("http://reddit.com/r/android", mTabModel.getInitialUrlForDocument(1011));
        // Same as in testBasic.
        assertEquals(false, mTabModel.isRetargetable(1010));
        assertEquals(false, mTabModel.isRetargetable(1011));

        // State of the tabs.
        assertTrue(mTabModel.isTabStateReady(1010));
        assertNotNull(mTabModel.getTabStateForDocument(1010));
        assertNull(mTabModel.getCurrentUrlForDocument(1010));
        assertNull(mTabModel.getCurrentUrlForDocument(1011));

        // Wait until the tab states are loaded.
        TestInitializationObserver.waitUntilState(
                mTabModel, DocumentTabModelImpl.STATE_LOAD_TAB_STATE_BG_END);
        assertNotNull(mTabModel.getTabStateForDocument(1011));
        // Same as in testBasic.
        assertEquals(true, mTabModel.isRetargetable(1010));
        assertEquals(false, mTabModel.isRetargetable(1011));

        // Load the native library, wait until the states are deserialized, then check their values.
        initializeNativeTabModel();
        assertEquals("http://www.erfworld.com/erf_stream/view",
                mTabModel.getCurrentUrlForDocument(1010));
        assertEquals("http://www.reddit.com/r/android", mTabModel.getCurrentUrlForDocument(1011));
    }

    /**
     * Tasks found in Android's Recents and not in the DocumentTabModel's task file should be
     * added to the DocumentTabModel.
     */
    @SmallTest
    public void testMissingTaskAddedAndUnretargetable() throws Exception {
        mActivityDelegate.addTask(false, 1012, "http://digg.com");
        mActivityDelegate.addTask(false, 1010, "http://erfworld.com");
        mActivityDelegate.addTask(false, 1011, "http://reddit.com/r/android");

        mStorageDelegate.setTaskFileBytesFromEncodedString(MODEL_STATE_WITH_1010_1011);

        setupDocumentTabModel();

        assertEquals(3, mTabModel.getCount());
        assertEquals(1012, mTabModel.getTabAt(0).getId());
        assertEquals(1010, mTabModel.getTabAt(1).getId());
        assertEquals(1011, mTabModel.getTabAt(2).getId());

        assertEquals("http://erfworld.com", mTabModel.getInitialUrlForDocument(1010));
        assertEquals("http://reddit.com/r/android", mTabModel.getInitialUrlForDocument(1011));
        assertEquals("http://digg.com", mTabModel.getInitialUrlForDocument(1012));

        // Wait until the tab states are loaded, by then the AsyncTask to load metadata is done.
        TestInitializationObserver.waitUntilState(
                mTabModel, DocumentTabModelImpl.STATE_LOAD_TAB_STATE_BG_END);
        assertEquals(true, mTabModel.isRetargetable(1010));
        assertEquals(false, mTabModel.isRetargetable(1011));
        assertEquals(false, mTabModel.isRetargetable(1012));
    }

    /**
     * If a TabState file is missing, we won't be able to get a current URL for it but should still
     * get notification that the TabState was loaded.
     */
    @SmallTest
    public void testMissingTabState() throws Exception {
        mActivityDelegate.addTask(false, 1010, "http://erfworld.com");
        mActivityDelegate.addTask(false, 1011, "http://reddit.com/r/android");

        mStorageDelegate.setTaskFileBytesFromEncodedString(MODEL_STATE_WITH_1010_1011);
        mStorageDelegate.addEncodedTabState(1011, false, TAB_STATE_1011_REDDIT);

        setupDocumentTabModel();

        assertEquals(2, mTabModel.getCount());
        assertTrue(mTabModel.isTabStateReady(1010));
        assertNull(mTabModel.getTabStateForDocument(1010));

        assertNull(mTabModel.getCurrentUrlForDocument(1010));
        assertNull(mTabModel.getCurrentUrlForDocument(1011));

        // After the DocumentTabModel has progressed far enough, confirm that the other available
        // TabState has been loaded.
        TestInitializationObserver.waitUntilState(
                mTabModel, DocumentTabModelImpl.STATE_LOAD_TAB_STATE_BG_END);
        assertTrue(mTabModel.isTabStateReady(1010));
        assertTrue(mTabModel.isTabStateReady(1011));

        assertNull(mTabModel.getTabStateForDocument(1010));
        assertNotNull(mTabModel.getTabStateForDocument(1011));

        // Load the native library, wait until the states are deserialized, then check their values.
        initializeNativeTabModel();
        assertNull(null, mTabModel.getCurrentUrlForDocument(1010));
        assertEquals("http://www.reddit.com/r/android", mTabModel.getCurrentUrlForDocument(1011));
    }

    @SmallTest
    public void testTasksSwipedAwayBeforeTabModelCreation() throws Exception {
        mActivityDelegate.addTask(false, 1010, "http://erfworld.com");

        mStorageDelegate.setTaskFileBytesFromEncodedString(MODEL_STATE_WITH_1010_1011);
        setupDocumentTabModel();

        assertEquals(1, mTabModel.getCount());
        assertEquals(1010, mTabModel.getTabAt(0).getId());
        assertEquals("http://erfworld.com", mTabModel.getInitialUrlForDocument(1010));
    }

    /**
     * Tasks swiped away in Android's Recents should be reflected as closed tabs in the
     * DocumentTabModel.
     */
    @SmallTest
    public void testTasksSwipedAwayAfterTabModelCreation() throws Exception {
        mActivityDelegate.addTask(false, 1010, "http://erfworld.com");
        mActivityDelegate.addTask(false, 1011, "http://reddit.com/r/android");

        mStorageDelegate.setTaskFileBytesFromEncodedString(MODEL_STATE_WITH_1010_1011);

        setupDocumentTabModel();
        assertEquals(2, mTabModel.getCount());
        assertEquals(1010, mTabModel.getTabAt(0).getId());
        assertEquals(1011, mTabModel.getTabAt(1).getId());

        mActivityDelegate.removeTask(false, 1010);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mTabModel.updateRecentlyClosed();
            }
        });
        assertEquals(1, mTabModel.getCount());
        assertEquals(1011, mTabModel.getTabAt(0).getId());
    }

    /**
     * DocumentTabModel#closeAllTabs() should remove all the tabs from the TabModel.
     */
    @SmallTest
    public void testCloseAllTabs() throws Exception {
        mActivityDelegate.addTask(false, 1010, "http://erfworld.com");
        mActivityDelegate.addTask(false, 1011, "http://reddit.com/r/android");

        mStorageDelegate.setTaskFileBytesFromEncodedString(MODEL_STATE_WITH_1010_1011);

        setupDocumentTabModel();
        assertEquals(2, mTabModel.getCount());
        assertEquals(1010, mTabModel.getTabAt(0).getId());
        assertEquals(1011, mTabModel.getTabAt(1).getId());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mTabModel.closeAllTabs();
            }
        });
        assertEquals(0, mTabModel.getCount());
    }

    /**
     * DocumentTabModel#closeTabAt() should close a tab and slide the other ones in to fill the gap.
     * This test also relies on the DocumentTabModel adding tasks it finds in Android's Recents to
     * pad out the test.
     */
    @SmallTest
    public void testCloseTabAt() throws Exception {
        mActivityDelegate.addTask(false, 1010, "http://erfworld.com");
        mActivityDelegate.addTask(false, 1011, "http://reddit.com/r/android");
        mActivityDelegate.addTask(false, 1012, "http://digg.com");
        mActivityDelegate.addTask(false, 1013, "http://slashdot.org");

        setupDocumentTabModel();
        assertEquals(4, mTabModel.getCount());
        assertEquals(1010, mTabModel.getTabAt(0).getId());
        assertEquals(1011, mTabModel.getTabAt(1).getId());
        assertEquals(1012, mTabModel.getTabAt(2).getId());
        assertEquals(1013, mTabModel.getTabAt(3).getId());

        assertTrue(CloseRunnable.closeTabAt(mTabModel, 1));
        assertEquals(3, mTabModel.getCount());
        assertEquals(1010, mTabModel.getTabAt(0).getId());
        assertEquals(1012, mTabModel.getTabAt(1).getId());
        assertEquals(1013, mTabModel.getTabAt(2).getId());

        assertTrue(CloseRunnable.closeTabAt(mTabModel, 2));
        assertEquals(2, mTabModel.getCount());
        assertEquals(1010, mTabModel.getTabAt(0).getId());
        assertEquals(1012, mTabModel.getTabAt(1).getId());

        assertTrue(CloseRunnable.closeTabAt(mTabModel, 0));
        assertEquals(1, mTabModel.getCount());
        assertEquals(1012, mTabModel.getTabAt(0).getId());

        assertTrue(CloseRunnable.closeTabAt(mTabModel, 0));
        assertEquals(0, mTabModel.getCount());

        assertFalse(CloseRunnable.closeTabAt(mTabModel, 0));
    }

    /**
     * Test that the DocumentTabModel.index() function works as expected as Tabs are selected and
     * closed.
     */
    @SmallTest
    public void testIndex() throws Exception {
        mActivityDelegate.addTask(false, 1010, "http://erfworld.com");
        mActivityDelegate.addTask(false, 1011, "http://reddit.com/r/android");
        mActivityDelegate.addTask(false, 1012, "http://digg.com");
        mActivityDelegate.addTask(false, 1013, "http://slashdot.org");

        Map<String, Object> data = new HashMap<String, Object>();
        data.put(DocumentTabModelImpl.PREF_LAST_SHOWN_TAB_ID_REGULAR, 1011);
        mContext.addSharedPreferences(DocumentTabModelImpl.PREF_PACKAGE, data);

        // The ID stored in the SharedPreferences points at index 1.
        setupDocumentTabModel();
        assertEquals(1, mTabModel.index());

        // Pick a different Tab.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                TabModelUtils.setIndex(mTabModel, 3);
            }
        });
        assertEquals(3, mTabModel.index());
        assertEquals(1013, mTabModel.getTabAt(3).getId());
        assertEquals(1013, data.get(DocumentTabModelImpl.PREF_LAST_SHOWN_TAB_ID_REGULAR));

        // Select the MRU tab since the last known Tab was closed. The last shown ID isn't updated
        // when the new Tab is selected; it's the job of the DocumentActivity to alert the
        // DocumentTabModel about shown Tab changes.
        assertTrue(CloseRunnable.closeTabAt(mTabModel, 3));
        assertEquals(3, mTabModel.getCount());
        assertEquals(0, mTabModel.index());
        assertEquals(1010, mTabModel.getTabAt(0).getId());

        // Close everything; index should be invalid.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mTabModel.closeAllTabs();
            }
        });
        assertEquals(0, mTabModel.getCount());
        assertEquals(TabModel.INVALID_TAB_INDEX, mTabModel.index());
    }

    /**
     * Test that we don't add information about a Tab that's not valid for a DocumentActivity.
     */
    @SmallTest
    public void testAddTab() throws Exception {
        setupDocumentTabModel();
        assertEquals(0, mTabModel.getCount());

        Intent badIntent = new Intent();
        badIntent.setData(Uri.parse("http://toteslegit.com"));
        Tab badTab = new Tab(Tab.INVALID_TAB_ID, false, null);
        mTabModel.addTab(badIntent, badTab);
        assertEquals(0, mTabModel.getCount());

        Intent legitIntent = new Intent();
        legitIntent.setData(Uri.parse("document://11684?http://erfworld.com"));
        Tab legitTab = new Tab(11684, false, null);
        mTabModel.addTab(legitIntent, legitTab);
        assertEquals(1, mTabModel.getCount());
    }
}

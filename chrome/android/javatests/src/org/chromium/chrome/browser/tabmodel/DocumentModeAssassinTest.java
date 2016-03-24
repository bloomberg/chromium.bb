// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.content.Context;
import android.test.MoreAsserts;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.preferences.DocumentModeManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.DocumentModeAssassin.DocumentModeAssassinObserver;
import org.chromium.chrome.browser.tabmodel.TabPersistentStoreTest.MockTabPersistentStoreObserver;
import org.chromium.chrome.browser.tabmodel.TabPersistentStoreTest.TestTabModelSelector;
import org.chromium.chrome.browser.tabmodel.TestTabModelDirectory.TabStateInfo;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModel;
import org.chromium.chrome.browser.tabmodel.document.MockDocumentTabModel;
import org.chromium.content.browser.test.NativeLibraryTestBase;
import org.chromium.content.browser.test.util.CallbackHelper;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Tests permanent migration from document mode to tabbed mode.
 *
 * This test is meant to run without the native library loaded, only loading it when confirming
 * that files have been written correctly.
 */
public class DocumentModeAssassinTest extends NativeLibraryTestBase {
    private static final String TAG = "DocumentModeAssassin";

    private static final String DOCUMENT_MODE_DIRECTORY_NAME = "ChromeDocumentActivity";
    private static final String TABBED_MODE_DIRECTORY_NAME = "app_tabs";
    private static final int TABBED_MODE_INDEX = 0;

    private static final TestTabModelDirectory.TabModelMetaDataInfo TEST_INFO =
            TestTabModelDirectory.TAB_MODEL_METADATA_V5_NO_M18;
    private static final TestTabModelDirectory.TabStateInfo[] TAB_STATE_INFO = TEST_INFO.contents;

    private TestTabModelDirectory mDocumentModeDirectory = null;
    private TestTabModelDirectory mTabbedModeDirectory = null;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mDocumentModeDirectory = new TestTabModelDirectory(
                getInstrumentation().getTargetContext(), DOCUMENT_MODE_DIRECTORY_NAME, null);
        mTabbedModeDirectory = new TestTabModelDirectory(
                getInstrumentation().getTargetContext(), TABBED_MODE_DIRECTORY_NAME,
                String.valueOf(TABBED_MODE_INDEX));
    }

    @Override
    public void tearDown() throws Exception {
        try {
            if (mDocumentModeDirectory != null) mDocumentModeDirectory.tearDown();
        } catch (Exception e) {
            Log.e(TAG, "Failed to clean up document mode directory.");
        }

        try {
            if (mTabbedModeDirectory != null) mTabbedModeDirectory.tearDown();
        } catch (Exception e) {
            Log.e(TAG, "Failed to clean up tabbed mode directory.");
        }

        super.tearDown();
    }

    private void writeUselessFileToDirectory(File directory, String filename) {
        File file = new File(directory, filename);
        FileOutputStream outputStream = null;
        try {
            outputStream = new FileOutputStream(file);
            outputStream.write(116);
        } catch (IOException e) {
            assert false : "Failed to create " + filename;
        } finally {
            StreamUtil.closeQuietly(outputStream);
        }
    }

    /**
     * Tests that the preference to knock a user out of document mode is properly set.
     */
    @SmallTest
    public void testForceToTabbedMode() throws Exception {
        final AdvancedMockContext context =
                new AdvancedMockContext(getInstrumentation().getTargetContext());
        final CallbackHelper changeStartedCallback = new CallbackHelper();
        final CallbackHelper changeDoneCallback = new CallbackHelper();
        final DocumentModeAssassin assassin = DocumentModeAssassin.createForTesting(
                DocumentModeAssassin.STAGE_WRITE_TABMODEL_METADATA_DONE);
        final DocumentModeAssassinObserver observer = new DocumentModeAssassinObserver() {
            @Override
            public void onStageChange(int newStage) {
                if (newStage == DocumentModeAssassin.STAGE_CHANGE_SETTINGS_STARTED) {
                    changeStartedCallback.notifyCalled();
                } else if (newStage == DocumentModeAssassin.STAGE_CHANGE_SETTINGS_DONE) {
                    changeDoneCallback.notifyCalled();
                }
            }
        };

        // Write out the preference and wait for it.
        assertEquals(0, changeStartedCallback.getCallCount());
        assertEquals(0, changeDoneCallback.getCallCount());
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                assassin.addObserver(observer);
                assassin.changePreferences(context);
            }
        });
        changeStartedCallback.waitForCallback(0);
        changeDoneCallback.waitForCallback(0);

        // Check that the preference was written out correctly.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertEquals(DocumentModeManager.OPTED_OUT_OF_DOCUMENT_MODE,
                        DocumentModeManager.getInstance(context).getOptOutStateForTesting());
            }
        });
    }

    /**
     * Tests that the {@link DocumentTabModel}'s data is properly saved out for a
     * {@link TabModelImpl}.
     */
    @MediumTest
    public void testWriteTabModelMetadata() throws Exception {
        // Make a DocumentTabModel that serves up tabs from our fake list.
        final DocumentTabModel testTabModel = new MockDocumentTabModel(false) {
            @Override
            public int getCount() {
                return TAB_STATE_INFO.length;
            }

            @Override
            public Tab getTabAt(int index) {
                return new Tab(TAB_STATE_INFO[index].tabId, false, null);
            }

            @Override
            public int index() {
                // Figure out which tab actually has the correct ID.
                for (int i = 0; i < TEST_INFO.contents.length; i++) {
                    if (TEST_INFO.selectedTabId == TEST_INFO.contents[i].tabId) return i;
                }
                fail();
                return TabModel.INVALID_TAB_INDEX;
            }

            @Override
            public String getInitialUrlForDocument(int tabId) {
                for (int i = 0; i < TAB_STATE_INFO.length; i++) {
                    if (TAB_STATE_INFO[i].tabId == tabId) {
                        return TAB_STATE_INFO[i].url;
                    }
                }
                fail();
                return null;
            }
        };

        // Write the TabState files into the tabbed mode directory directly, but fail to copy just
        // one of them.  This forces the TabPersistentStore to improvise and use the initial URL
        // that we provide.
        final TabStateInfo failureCase = TestTabModelDirectory.V2_HAARETZ;
        final Set<Integer> migratedTabIds = new HashSet<Integer>();
        for (int i = 0; i < TAB_STATE_INFO.length; i++) {
            if (failureCase.tabId == TAB_STATE_INFO[i].tabId) continue;
            migratedTabIds.add(TAB_STATE_INFO[i].tabId);
            mTabbedModeDirectory.writeTabStateFile(TAB_STATE_INFO[i]);
        }

        // Start writing the data out.
        final CallbackHelper writeStartedCallback = new CallbackHelper();
        final CallbackHelper writeDoneCallback = new CallbackHelper();
        final DocumentModeAssassin assassin = DocumentModeAssassin.createForTesting(
                DocumentModeAssassin.STAGE_COPY_TAB_STATES_DONE);
        final DocumentModeAssassinObserver observer = new DocumentModeAssassinObserver() {
            @Override
            public void onStageChange(int newStage) {
                if (newStage == DocumentModeAssassin.STAGE_WRITE_TABMODEL_METADATA_STARTED) {
                    writeStartedCallback.notifyCalled();
                } else if (newStage == DocumentModeAssassin.STAGE_WRITE_TABMODEL_METADATA_DONE) {
                    writeDoneCallback.notifyCalled();
                }
            }
        };

        File[] tabbedModeFilesBefore = mTabbedModeDirectory.getDataDirectory().listFiles();
        assertNotNull(tabbedModeFilesBefore);
        int numFilesBefore = tabbedModeFilesBefore.length;
        assertEquals(0, writeStartedCallback.getCallCount());
        assertEquals(0, writeDoneCallback.getCallCount());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assassin.addObserver(observer);
                assassin.writeTabModelMetadata(
                        mTabbedModeDirectory.getDataDirectory(), testTabModel, migratedTabIds);
            }
        });

        // Wait and confirm that the tabbed mode metadata file was written out.
        writeStartedCallback.waitForCallback(0);
        writeDoneCallback.waitForCallback(0);
        File[] tabbedModeFilesAfter = mTabbedModeDirectory.getDataDirectory().listFiles();
        assertNotNull(tabbedModeFilesAfter);
        assertEquals(numFilesBefore + 1, tabbedModeFilesAfter.length);

        // Load up the metadata file via a TabPersistentStore to make sure that it contains all of
        // the migrated tab information.
        loadNativeLibraryAndInitBrowserProcess();
        TabPersistentStore.setBaseStateDirectory(mTabbedModeDirectory.getBaseDirectory());

        Context context = getInstrumentation().getTargetContext();
        TestTabModelSelector selector = new TestTabModelSelector(context);
        TabPersistentStore store = selector.mTabPersistentStore;
        MockTabPersistentStoreObserver mockObserver = selector.mTabPersistentStoreObserver;

        // Load up the TabModel metadata.
        int numExpectedTabs = TEST_INFO.numRegularTabs + TEST_INFO.numIncognitoTabs;
        store.loadState();
        mockObserver.initializedCallback.waitForCallback(0, 1);
        assertEquals(numExpectedTabs, mockObserver.mTabCountAtStartup);
        mockObserver.detailsReadCallback.waitForCallback(0, TEST_INFO.contents.length);
        assertEquals(numExpectedTabs, mockObserver.details.size());

        // Restore the TabStates, then confirm that things were restored correctly, in the right tab
        // order and with the right URLs.
        store.restoreTabs(true);
        mockObserver.stateLoadedCallback.waitForCallback(0, 1);
        assertEquals(TEST_INFO.numRegularTabs, selector.getModel(false).getCount());
        assertEquals(TEST_INFO.numIncognitoTabs, selector.getModel(true).getCount());

        for (int i = 0; i < numExpectedTabs; i++) {
            int savedTabId = TEST_INFO.contents[i].tabId;
            int restoredId = selector.getModel(false).getTabAt(i).getId();

            if (failureCase.tabId == savedTabId) {
                // The Tab without a written TabState file will get a new Tab ID.
                MoreAsserts.assertNotEqual(failureCase.tabId, restoredId);
            } else {
                // Restored Tabs get the ID that they expected.
                assertEquals(savedTabId, restoredId);
            }

            // The URL wasn't written into the metadata file, so this will be correct only if
            // the TabPersistentStore successfully read the TabState files we planted earlier.
            // In the case where we intentionally didn't copy the TabState file, the metadata file
            // will contain the URL that the DocumentActivity was initially launched for, which gets
            // used as the fallback.
            assertEquals(TEST_INFO.contents[i].url, selector.getModel(false).getTabAt(i).getUrl());
        }
    }

    /** Checks that all TabState files are copied successfully. */
    @MediumTest
    public void testCopyTabStateFiles() throws Exception {
        performCopyTest(Tab.INVALID_TAB_ID);
    }

    /** Confirms that the selected tab's TabState file is copied before all the other ones. */
    @MediumTest
    public void testSelectedTabCopiedFirst() throws Exception {
        performCopyTest(TestTabModelDirectory.V2_HAARETZ.tabId);
    }

    private void performCopyTest(final int selectedTabId) throws Exception {
        final CallbackHelper copyStartedCallback = new CallbackHelper();
        final CallbackHelper copyDoneCallback = new CallbackHelper();
        final CallbackHelper copyCallback = new CallbackHelper();
        final AtomicInteger firstCopiedId = new AtomicInteger(Tab.INVALID_TAB_ID);
        final ArrayList<Integer> copiedIds = new ArrayList<Integer>();
        final DocumentModeAssassin assassin = DocumentModeAssassin.createForTesting(
                DocumentModeAssassin.STAGE_INITIALIZED);

        final DocumentModeAssassinObserver observer = new DocumentModeAssassinObserver() {
            @Override
            public void onStageChange(int newStage) {
                if (newStage == DocumentModeAssassin.STAGE_COPY_TAB_STATES_STARTED) {
                    copyStartedCallback.notifyCalled();
                } else if (newStage == DocumentModeAssassin.STAGE_COPY_TAB_STATES_DONE) {
                    copyDoneCallback.notifyCalled();
                }
            }

            @Override
            public void onTabStateFileCopied(int copiedId) {
                if (firstCopiedId.get() == Tab.INVALID_TAB_ID) firstCopiedId.set(copiedId);
                copiedIds.add(copiedId);
                copyCallback.notifyCalled();
            }
        };

        // Write out all of the TabState files into the document mode directory.
        for (int i = 0; i < TAB_STATE_INFO.length; i++) {
            mDocumentModeDirectory.writeTabStateFile(TAB_STATE_INFO[i]);
        }

        // Write out some random files that should be ignored by migration.
        writeUselessFileToDirectory(mDocumentModeDirectory.getDataDirectory(), "ignored.file");
        writeUselessFileToDirectory(mDocumentModeDirectory.getDataDirectory(),
                TabState.SAVED_TAB_STATE_FILE_PREFIX + "_unparseable");

        // Kick off copying the tab states.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assassin.addObserver(observer);
                assertEquals(0, copyStartedCallback.getCallCount());
                assertEquals(0, copyDoneCallback.getCallCount());
                assertEquals(0, copyCallback.getCallCount());
                assassin.copyTabStateFiles(selectedTabId, mDocumentModeDirectory.getDataDirectory(),
                        mTabbedModeDirectory.getDataDirectory());
            }
        });
        copyStartedCallback.waitForCallback(0);

        // Confirm that the first TabState file copied back is the selected one.
        copyCallback.waitForCallback(0);
        if (selectedTabId != Tab.INVALID_TAB_ID) assertEquals(selectedTabId, firstCopiedId.get());

        // Confirm that all the TabState files were copied over.
        copyDoneCallback.waitForCallback(0);
        assertEquals(TAB_STATE_INFO.length, copyCallback.getCallCount());
        assertEquals(TAB_STATE_INFO.length, copiedIds.size());
        for (int i = 0; i < TAB_STATE_INFO.length; i++) {
            assertTrue(copiedIds.contains(TAB_STATE_INFO[i].tabId));
        }

        // Confirm that the legitimate TabState files were all copied over.
        File[] tabbedModeFilesAfter = mTabbedModeDirectory.getDataDirectory().listFiles();
        assertNotNull(tabbedModeFilesAfter);
        assertEquals(TAB_STATE_INFO.length, tabbedModeFilesAfter.length);

        for (int i = 0; i < TAB_STATE_INFO.length; i++) {
            boolean found = false;
            for (int j = 0; j < tabbedModeFilesAfter.length && !found; j++) {
                found |= TAB_STATE_INFO[i].filename.equals(tabbedModeFilesAfter[j].getName());
            }
            assertTrue("Couldn't find file: " + TAB_STATE_INFO[i].filename, found);
        }
    }
}
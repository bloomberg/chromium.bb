// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.StreamUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.test.util.ApplicationData;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Test that migrating the old tab state folder structure to the new one works.
 */
public class RestoreMigrateTest extends InstrumentationTestCase {

    private void writeStateFile(final TabModelSelector selector, int index) throws IOException {
        byte[] data = ThreadUtils.runOnUiThreadBlockingNoException(
                new Callable<byte[]>() {
                    @Override
                    public byte[] call() throws Exception {
                        return TabPersistentStore.serializeTabModelSelector(selector, null);
                    }
                });
        File f = TabPersistentStore.getStateDirectory(
                getInstrumentation().getTargetContext(), index);
        FileOutputStream fos = null;
        try {
            fos = new FileOutputStream(new File(f, TabPersistentStore.SAVED_STATE_FILE));
            fos.write(data);
        } finally {
            StreamUtil.closeQuietly(fos);
        }
    }

    private int getMaxId(TabModelSelector selector) {
        int maxId = 0;
        for (TabList list : selector.getModels()) {
            for (int i = 0; i < list.getCount(); i++) {
                maxId = Math.max(maxId, list.getTabAt(i).getId());
            }
        }
        return maxId;
    }

    /**
     * Test that normal migration of state files works.
     * @throws IOException
     * @throws InterruptedException
     * @throws ExecutionException
     */
    @SuppressWarnings("unused")
    @SuppressFBWarnings("DLS_DEAD_LOCAL_STORE")
    @SmallTest
    public void testMigrateData() throws IOException, InterruptedException, ExecutionException {
        ApplicationData.clearAppData(getInstrumentation().getTargetContext());

        // Write old state files.
        File filesDir = getInstrumentation().getTargetContext().getFilesDir();
        File stateFile = new File(filesDir, TabPersistentStore.SAVED_STATE_FILE);
        File tab0 = new File(filesDir, TabState.SAVED_TAB_STATE_FILE_PREFIX + "0");
        File tab1 = new File(filesDir, TabState.SAVED_TAB_STATE_FILE_PREFIX + "1");
        File tab2 = new File(filesDir, TabState.SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO + "2");
        File tab3 = new File(filesDir, TabState.SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO + "3");

        assertTrue("Could not create state file", stateFile.createNewFile());
        assertTrue("Could not create tab 0 file", tab0.createNewFile());
        assertTrue("Could not create tab 1 file", tab1.createNewFile());
        assertTrue("Could not create tab 2 file", tab2.createNewFile());
        assertTrue("Could not create tab 3 file", tab3.createNewFile());

        // Build the TabPersistentStore which will try to move the files.
        MockTabModelSelector selector = new MockTabModelSelector(0, 0, null);
        TabPersistentStore store = new TabPersistentStore(selector, 0,
                getInstrumentation().getTargetContext(), null, null);
        TabPersistentStore.waitForMigrationToFinish();

        // Check that the files were moved.
        File newDir =
                TabPersistentStore.getStateDirectory(getInstrumentation().getTargetContext(), 0);
        File newStateFile = new File(newDir, TabPersistentStore.SAVED_STATE_FILE);
        File newTab0 = new File(newDir, TabState.SAVED_TAB_STATE_FILE_PREFIX + "0");
        File newTab1 = new File(newDir, TabState.SAVED_TAB_STATE_FILE_PREFIX + "1");
        File newTab2 = new File(newDir, TabState.SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO + "2");
        File newTab3 = new File(newDir, TabState.SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO + "3");

        assertTrue("Could not find new state file", newStateFile.exists());
        assertTrue("Could not find new tab 0 file", newTab0.exists());
        assertTrue("Could not find new tab 1 file", newTab1.exists());
        assertTrue("Could not find new tab 2 file", newTab2.exists());
        assertTrue("Could not find new tab 3 file", newTab3.exists());

        assertFalse("Could still find old state file", stateFile.exists());
        assertFalse("Could still find old tab 0 file", tab0.exists());
        assertFalse("Could still find old tab 1 file", tab1.exists());
        assertFalse("Could still find old tab 2 file", tab2.exists());
        assertFalse("Could still find old tab 3 file", tab3.exists());

        ApplicationData.clearAppData(getInstrumentation().getTargetContext());
    }

    /**
     * Test that migration skips if it already has files in the new folder.
     * @throws IOException
     * @throws InterruptedException
     * @throws ExecutionException
     */
    @SuppressWarnings("unused")
    @SuppressFBWarnings("DLS_DEAD_LOCAL_STORE")
    @SmallTest
    public void testSkipMigrateData() throws IOException, InterruptedException, ExecutionException {
        ApplicationData.clearAppData(getInstrumentation().getTargetContext());

        // Write old state files.
        File filesDir = getInstrumentation().getTargetContext().getFilesDir();
        File stateFile = new File(filesDir, TabPersistentStore.SAVED_STATE_FILE);
        File tab0 = new File(filesDir, TabState.SAVED_TAB_STATE_FILE_PREFIX + "0");
        File tab1 = new File(filesDir, TabState.SAVED_TAB_STATE_FILE_PREFIX + "1");
        File tab2 = new File(filesDir, TabState.SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO + "2");
        File tab3 = new File(filesDir, TabState.SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO + "3");

        assertTrue("Could not create state file", stateFile.createNewFile());
        assertTrue("Could not create tab 0 file", tab0.createNewFile());
        assertTrue("Could not create tab 1 file", tab1.createNewFile());
        assertTrue("Could not create tab 2 file", tab2.createNewFile());
        assertTrue("Could not create tab 3 file", tab3.createNewFile());

        // Write new state files
        File newDir =
                TabPersistentStore.getStateDirectory(getInstrumentation().getTargetContext(), 0);
        File newStateFile = new File(newDir, TabPersistentStore.SAVED_STATE_FILE);
        File newTab4 = new File(newDir, TabState.SAVED_TAB_STATE_FILE_PREFIX + "4");

        assertTrue("Could not create new tab 4 file", newTab4.createNewFile());
        assertTrue("Could not create new state file", newStateFile.createNewFile());

        // Build the TabPersistentStore which will try to move the files.
        MockTabModelSelector selector = new MockTabModelSelector(0, 0, null);
        TabPersistentStore store = new TabPersistentStore(selector, 0,
                getInstrumentation().getTargetContext(), null, null);
        TabPersistentStore.waitForMigrationToFinish();

        assertTrue("Could not find new state file", newStateFile.exists());
        assertTrue("Could not find new tab 4 file", newTab4.exists());

        // Make sure the old files did not move
        File newTab0 = new File(newDir, TabState.SAVED_TAB_STATE_FILE_PREFIX + "0");
        File newTab1 = new File(newDir, TabState.SAVED_TAB_STATE_FILE_PREFIX + "1");
        File newTab2 = new File(newDir, TabState.SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO + "2");
        File newTab3 = new File(newDir, TabState.SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO + "3");

        assertFalse("Could find new tab 0 file", newTab0.exists());
        assertFalse("Could find new tab 1 file", newTab1.exists());
        assertFalse("Could find new tab 2 file", newTab2.exists());
        assertFalse("Could find new tab 3 file", newTab3.exists());

        ApplicationData.clearAppData(getInstrumentation().getTargetContext());
    }

    /**
     * Test that the state file migration skips unrelated files.
     * @throws IOException
     * @throws InterruptedException
     * @throws ExecutionException
     */
    @SuppressWarnings("unused")
    @SuppressFBWarnings("DLS_DEAD_LOCAL_STORE")
    @SmallTest
    public void testMigrationLeavesOtherFilesAlone() throws IOException, InterruptedException,
            ExecutionException {
        ApplicationData.clearAppData(getInstrumentation().getTargetContext());

        // Write old state files.
        File filesDir = getInstrumentation().getTargetContext().getFilesDir();
        File stateFile = new File(filesDir, TabPersistentStore.SAVED_STATE_FILE);
        File tab0 = new File(filesDir, TabState.SAVED_TAB_STATE_FILE_PREFIX + "0");
        File otherFile = new File(filesDir, "other.file");

        assertTrue("Could not create state file", stateFile.createNewFile());
        assertTrue("Could not create tab 0 file", tab0.createNewFile());
        assertTrue("Could not create other file", otherFile.createNewFile());

        // Build the TabPersistentStore which will try to move the files.
        MockTabModelSelector selector = new MockTabModelSelector(0, 0, null);
        TabPersistentStore store = new TabPersistentStore(selector, 0,
                getInstrumentation().getTargetContext(), null, null);
        TabPersistentStore.waitForMigrationToFinish();

        assertFalse("Could still find old state file", stateFile.exists());
        assertFalse("Could still find old tab 0 file", tab0.exists());
        assertTrue("Could not find other file", otherFile.exists());

        // Check that the files were moved.
        File newDir =
                TabPersistentStore.getStateDirectory(getInstrumentation().getTargetContext(), 0);
        File newStateFile = new File(newDir, TabPersistentStore.SAVED_STATE_FILE);
        File newTab0 = new File(newDir, TabState.SAVED_TAB_STATE_FILE_PREFIX + "0");
        File newOtherFile = new File(newDir, "other.file");

        assertTrue("Could not find new state file", newStateFile.exists());
        assertTrue("Could not find new tab 0 file", newTab0.exists());
        assertFalse("Could find new other file", newOtherFile.exists());

        ApplicationData.clearAppData(getInstrumentation().getTargetContext());
    }

    /**
     * Tests that the max id returned is the max of all of the tab models.
     * @throws IOException
     */
    @SmallTest
    public void testFindsMaxIdProperly() throws IOException {
        TabModelSelector selector0 = new MockTabModelSelector(1, 1, null);
        TabModelSelector selector1 = new MockTabModelSelector(1, 1, null);

        writeStateFile(selector0, 0);
        writeStateFile(selector1, 1);

        TabModelSelector selectorIn = new MockTabModelSelector(0, 0, null);
        TabPersistentStore storeIn = new TabPersistentStore(selectorIn, 0,
                getInstrumentation().getTargetContext(), null, null);

        int maxId = Math.max(getMaxId(selector0), getMaxId(selector1));
        RecordHistogram.disableForTests();
        assertEquals("Invalid next id", maxId + 1, storeIn.loadStateInternal());
    }

    /**
     * Tests that each model loads the subset of tabs it is responsible for.  In this case, just
     * check that the model has the expected number of tabs to load.  Since each model is loading
     * a different number of tabs we can tell if they are each attempting to load their specific
     * set.
     * @throws IOException
     */
    @SmallTest
    public void testOnlyLoadsSingleModel() throws IOException {
        TabModelSelector selector0 = new MockTabModelSelector(3, 3, null);
        TabModelSelector selector1 = new MockTabModelSelector(2, 1, null);

        writeStateFile(selector0, 0);
        writeStateFile(selector1, 1);

        TabModelSelector selectorIn0 = new MockTabModelSelector(0, 0, null);
        TabModelSelector selectorIn1 = new MockTabModelSelector(0, 0, null);

        TabPersistentStore storeIn0 = new TabPersistentStore(selectorIn0, 0,
                getInstrumentation().getTargetContext(), null, null);
        TabPersistentStore storeIn1 = new TabPersistentStore(selectorIn1, 1,
                getInstrumentation().getTargetContext(), null, null);

        RecordHistogram.disableForTests();
        storeIn0.loadStateInternal();
        storeIn1.loadStateInternal();

        assertEquals("Unexpected number of tabs to load", 6, storeIn0.getRestoredTabCount());
        assertEquals("Unexpected number of tabst o load", 3, storeIn1.getRestoredTabCount());

    }
}

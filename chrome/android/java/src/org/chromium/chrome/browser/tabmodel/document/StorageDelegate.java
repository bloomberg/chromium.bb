// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel.document;

import android.content.Context;
import android.util.Log;

import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.util.StreamUtil;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;

/**
 * Contains functions for interacting with the file system.
 */
public class StorageDelegate {
    private static final String TAG = "StorageDelegate";

    /** Filename to use for the DocumentTabModel that stores regular tabs. */
    private static final String REGULAR_FILE_NAME = "chrome_document_activity.store";

    /** Directory to store TabState files in. */
    private static final String STATE_DIRECTORY = "ChromeDocumentActivity";

    /** The buffer size to use when reading the DocumentTabModel file, set to 4k bytes. */
    private static final int BUF_SIZE = 0x1000;

    /** Whether this is dealing with incognito state. */
    protected final boolean mIsIncognito;

    public StorageDelegate(boolean isIncognito) {
        mIsIncognito = isIncognito;
    }

    /**
     * Reads the file containing the minimum info required to restore the state of the
     * {@link DocumentTabModel}.
     * @return Byte buffer containing the task file's data, or null if it wasn't read.
     */
    public byte[] readTaskFileBytes() {
        // Incognito mode doesn't save its state out.
        if (mIsIncognito) return null;

        // Read in the file.
        byte[] bytes = null;
        FileInputStream streamIn = null;
        try {
            String filename = getFilename();
            streamIn = ApplicationStatus.getApplicationContext().openFileInput(filename);

            // Read the file from the file into the out stream.
            ByteArrayOutputStream streamOut = new ByteArrayOutputStream();
            byte[] buf = new byte[BUF_SIZE];
            int r;
            while ((r = streamIn.read(buf)) != -1) {
                streamOut.write(buf, 0, r);
            }
            bytes = streamOut.toByteArray();
        } catch (FileNotFoundException e) {
            Log.e(TAG, "DocumentTabModel file not found.");
        } catch (IOException e) {
            Log.e(TAG, "I/O exception", e);
        } finally {
            StreamUtil.closeQuietly(streamIn);
        }

        return bytes;
    }

    /**
     * Writes the file containing the minimum info required to restore the state of the
     * {@link DocumentTabModel}.
     * @param isIncognito Whether the TabModel is incognito.
     * @param bytes Byte buffer containing the tab's data.
     */
    public void writeTaskFileBytes(byte[] bytes) {
        // Incognito mode doesn't save its state out.
        if (mIsIncognito) return;

        FileOutputStream outputStream = null;
        try {
            outputStream = ApplicationStatus.getApplicationContext().openFileOutput(
                    getFilename(), Context.MODE_PRIVATE);
            outputStream.write(bytes);
        } catch (FileNotFoundException e) {
            Log.e(TAG, "DocumentTabModel file not found", e);
        } catch (IOException e) {
            Log.e(TAG, "I/O exception", e);
        } finally {
            StreamUtil.closeQuietly(outputStream);
        }
    }

    /** @return The directory that stores the TabState files. */
    public File getStateDirectory() {
        return ApplicationStatus.getApplicationContext().getDir(
                STATE_DIRECTORY, Context.MODE_PRIVATE);
    }

    /**
     * Restores the TabState with the given ID.
     * @param tabId ID of the Tab.
     * @return TabState for the Tab.
     */
    public TabState restoreTabState(int tabId) {
        return TabState.restoreTabState(getTabFile(tabId), mIsIncognito);
    }

    /**
     * Saves the TabState with the given ID.
     * @param tabId ID of the Tab.
     * @param state TabState for the Tab.
     */
    public void saveTabState(int tabId, TabState state) {
        FileOutputStream stream = null;
        try {
            stream = new FileOutputStream(getTabFile(tabId));
            TabState.saveState(stream, state, mIsIncognito);
        } catch (FileNotFoundException exception) {
            Log.e(TAG, "Failed to save out tab state for tab " + tabId, exception);
        } catch (IOException exception) {
            Log.e(TAG, "Failed to save out tab state.", exception);
        } finally {
            StreamUtil.closeQuietly(stream);
        }
    }

    /**
     * Deletes the TabState file for the given ID.
     * @param tabId ID of the TabState file to delete.
     */
    public void deleteTabStateFile(int tabId) {
        boolean success = getTabFile(tabId).delete();
        if (!success) Log.w(TAG, "Failed to delete file for tab " + tabId);
    }

    private File getTabFile(int tabId) {
        String tabStateFilename = TabState.getTabStateFilename(tabId, mIsIncognito);
        return new File(getStateDirectory(), tabStateFilename);
    }

    /** @return the filename of the persisted TabModel state. */
    private String getFilename() {
        return mIsIncognito ? null : REGULAR_FILE_NAME;
    }
}
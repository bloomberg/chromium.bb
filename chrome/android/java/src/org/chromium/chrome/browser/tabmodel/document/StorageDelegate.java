// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel.document;

import android.content.Context;
import android.util.Log;

import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.tabmodel.TabPersister;
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
public class StorageDelegate extends TabPersister {
    private static final String TAG = "StorageDelegate";

    /** Filename to use for the DocumentTabModel that stores regular tabs. */
    private static final String REGULAR_FILE_NAME = "chrome_document_activity.store";

    /** Directory to store TabState files in. */
    private static final String STATE_DIRECTORY = "ChromeDocumentActivity";

    /** The buffer size to use when reading the DocumentTabModel file, set to 4k bytes. */
    private static final int BUF_SIZE = 0x1000;

    /**
     * Reads the file containing the minimum info required to restore the state of the
     * {@link DocumentTabModel}.
     * @param encrypted Whether or not the file corresponds to an OffTheRecord TabModel.
     * @return Byte buffer containing the task file's data, or null if it wasn't read.
     */
    public byte[] readTaskFileBytes(boolean encrypted) {
        // Incognito mode doesn't save its state out.
        if (encrypted) return null;

        // Read in the file.
        byte[] bytes = null;
        FileInputStream streamIn = null;
        try {
            String filename = getFilename(encrypted);
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
     * @param encrypted Whether the TabModel is incognito.
     * @param bytes Byte buffer containing the tab's data.
     */
    public void writeTaskFileBytes(boolean encrypted, byte[] bytes) {
        // Incognito mode doesn't save its state out.
        if (encrypted) return;

        FileOutputStream outputStream = null;
        try {
            outputStream = ApplicationStatus.getApplicationContext().openFileOutput(
                    getFilename(encrypted), Context.MODE_PRIVATE);
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
    @Override
    public File getStateDirectory() {
        return ApplicationStatus.getApplicationContext().getDir(
                STATE_DIRECTORY, Context.MODE_PRIVATE);
    }

    /**
     * Restores the TabState with the given ID.
     * @param tabId ID of the Tab.
     * @return TabState for the Tab.
     */
    public TabState restoreTabState(int tabId, boolean encrypted) {
        return TabState.restoreTabState(getTabStateFile(tabId, encrypted), encrypted);
    }

    /**
     * Return the filename of the persisted TabModel state.
     * @param encrypted Whether or not the state belongs to an OffTheRecordDocumentTabModel.
     * @return String pointing at the TabModel's persisted state.
     */
    private String getFilename(boolean encrypted) {
        return encrypted ? null : REGULAR_FILE_NAME;
    }
}
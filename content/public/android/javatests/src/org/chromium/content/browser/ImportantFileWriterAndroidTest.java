// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.support.test.filters.SmallTest;

import org.chromium.base.ImportantFileWriterAndroid;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.NativeLibraryTestBase;

import java.io.DataInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;


/**
 * Tests for {@Link ImportantFileWriterAndroid}
 *
 * Note that this assumes that the underlying native atomic write functions
 * work, so is not attempting to test that writes are atomic. Instead it is just
 * testing that the Java code is calling the native code correctly.
 */
public class ImportantFileWriterAndroidTest extends NativeLibraryTestBase {

    private void checkFile(File testFile, byte[] data) {
        assertTrue(testFile.exists());
        try {
            byte[] fileData = new byte[(int) testFile.length()];
            DataInputStream dis =
                    new DataInputStream(new FileInputStream(testFile));
            dis.readFully(fileData);
            dis.close();
            assertEquals("Data length wrong", data.length, fileData.length);
            for (int i = 0; i < data.length; i++) {
                assertEquals("Data byte wrong", data[i], fileData[i]);
            }
        } catch (IOException e) {
            fail("Failed to read file");
        }

    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testAtomicWrite() {
        // Try writing a file that can't be created.
        byte[] data1 = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        assertFalse("Writing bad file succeded",
                ImportantFileWriterAndroid.writeFileAtomically(
                        "/junk/junk", data1));
        File dir = getInstrumentation().getTargetContext().getFilesDir();
        File testFile = new File(dir, "ImportantFileTest");

        // Make sure the file doesn't already exist
        if (testFile.exists()) {
            assertTrue(testFile.delete());
        }

        // Write a new file
        assertTrue("Writing new file failed",
                ImportantFileWriterAndroid.writeFileAtomically(
                        testFile.getAbsolutePath(), data1));
        checkFile(testFile, data1);
        byte[] data2 = {10, 20, 30, 40, 50, 60, 70, 80};

        // Overwrite an existing file
        assertTrue("Writing exiting file failed",
                ImportantFileWriterAndroid.writeFileAtomically(
                        testFile.getAbsolutePath(), data2));
        checkFile(testFile, data2);

        // Done, tidy up
        assertTrue(testFile.delete());
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        loadNativeLibraryNoBrowserProcess();
    }
}

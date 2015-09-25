// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import java.io.File;

/**
 * Helper methods for dealing with Files.
 */
public class FileUtils {
    private static final String TAG = "FileUtils";

    /**
     * Delete the given File and (if it's a directory) everything within it.
     */
    public static void recursivelyDeleteFile(File currentFile) {
        assert !ThreadUtils.runningOnUiThread();
        if (currentFile.isDirectory()) {
            File[] files = currentFile.listFiles();
            if (files != null) {
                for (File file : files) {
                    recursivelyDeleteFile(file);
                }
            }
        }

        if (!currentFile.delete()) Log.e(TAG, "Failed to delete: " + currentFile);
    }
}

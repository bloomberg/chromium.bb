// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.content.res.AssetManager;
import android.util.Log;

import org.chromium.base.PathUtils;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Helper class to install test files.
 */
public final class TestFilesInstaller {
    private static final String TAG = "TestFilesInstaller";
    // Name of the asset directory in which test files are stored.
    private static final String TEST_FILE_ASSET_PATH = "test";

    /**
     * Installs test files if files have not been installed.
     */
    public static void installIfNeeded(Context context) {
        if (areFilesInstalled(context)) {
            return;
        }
        install(context);
    }

    /**
     * Returns the installed path of the test files.
     */
    public static String getInstalledPath(Context context) {
        return PathUtils.getDataDirectory(context) + "/test";
    }

    /**
     * Returns whether test files are installed.
     */
    public static boolean areFilesInstalled(Context context) {
        // Checking for file directory is fine even when new files are added,
        // because the app will be re-installed and app data will be cleared.
        File directory = new File(getInstalledPath(context));
        return directory.exists();
    }

    /**
     * Installs test files that are included in assets.
     * @params context Application context
     */
    private static void install(Context context) {
        AssetManager assetManager = context.getAssets();
        try {
            String[] files = assetManager.list(TEST_FILE_ASSET_PATH);
            String destDir = getInstalledPath(context);
            File destDirFile = new File(destDir);
            if (!destDirFile.mkdir()) {
                throw new IllegalStateException(
                        "directory exists or it cannot be created.");
            }
            Log.i(TAG, "Begin loading " + files.length + " test files.");
            for (String fileName : files) {
                Log.i(TAG, "Loading " + fileName);
                String destFilePath = destDir + "/" + fileName;
                if (!copyTestFile(assetManager,
                                 TEST_FILE_ASSET_PATH + "/" + fileName,
                                 destFilePath)) {
                    Log.e(TAG, "Loading " + fileName + " failed.");
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    /**
     * Copies a file from assets to the device's file system.
     * @param assetManager AssetManager of the application.
     * @param srcFilePath the source file path in assets.
     * @param destFilePath the destination file path.
     * @throws IllegalStateException if the destination file already exists.
     */
    private static boolean copyTestFile(AssetManager assetManager,
                                        String srcFilePath,
                                        String destFilePath) {
        OutputStream out;
        try {
            File destFile = new File(destFilePath);
            if (destFile.exists()) {
                throw new IllegalStateException(srcFilePath
                        + " already exists");
            }
            out = new FileOutputStream(destFilePath);
        } catch (Exception e) {
            e.printStackTrace();
            return false;
        }
        try {
            InputStream in = assetManager.open(srcFilePath);

            byte[] buffer = new byte[1024];
            int read;
            while ((read = in.read(buffer)) != -1) {
                out.write(buffer, 0, read);
            }
            in.close();
            out.flush();
            out.close();
            return true;
        } catch (Exception e) {
            try {
                out.close();
            } catch (Exception closeException) {
                closeException.printStackTrace();
            }
            e.printStackTrace();
            return false;
        }
    }
}

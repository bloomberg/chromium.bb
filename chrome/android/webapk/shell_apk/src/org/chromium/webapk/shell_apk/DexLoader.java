// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.Context;
import android.util.Log;

import dalvik.system.BaseDexClassLoader;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

/**
 * Creates ClassLoader for .dex file in a remote Context's APK.
 */
public class DexLoader {
    private static final int BUFFER_SIZE = 16 * 1024;
    private static final String TAG = "cr.DexLoader";

    /**
     * Creates ClassLoader for .dex file in {@link remoteContext}'s APK.
     * @param remoteContext The context with the APK with the .dex file.
     * @param dexName The name of the .dex file in the APK.
     * @param canaryClassName Name of class in the .dex file. Used for testing the ClassLoader
     *                        before returning it.
     * @param remoteDexFile Location of already extracted .dex file from APK.
     * @param localDexDir Writable directory for caching data to speed up future calls to
     *                    {@link #load()}.
     * @return The ClassLoader. Returns null on an error.
     */
    public static ClassLoader load(Context remoteContext, String dexName, String canaryClassName,
            File remoteDexFile, File localDexDir) {
        File localDexFile = new File(localDexDir, dexName);

        // If {@link localDexFile} exists, technique #2 was previously used to create the
        // ClassLoader. Skip right to the technique which worked last time.
        if (remoteDexFile != null && remoteDexFile.exists() && !localDexFile.exists()) {
            // Technique #1: At startup, Chrome code extracts the .dex file from its assets and
            // guesses where DexClassLoader searches for the odex by default and puts the odex
            // there. Try using the odex from Chrome's data directory.
            ClassLoader loader = tryCreatingClassLoader(canaryClassName, remoteDexFile, null);
            if (loader != null) {
                return loader;
            }
        }

        // Technique #2: Extract the .dex file from the remote context's APK. Create a
        // ClassLoader from the extracted file.
        if (!localDexFile.exists() || localDexFile.length() == 0) {
            if (!localDexDir.exists() && !localDexDir.mkdirs()) {
                return null;
            }

            InputStream inputStream;
            try {
                inputStream = remoteContext.getAssets().open(dexName);
            } catch (Exception e) {
                Log.w(TAG, "Could not extract dex from assets");
                e.printStackTrace();
                return null;
            }
            copyFromInputStream(inputStream, localDexFile);
        }

        File localOptimizedDir = new File(localDexDir, "optimized");
        if (!localOptimizedDir.exists() && !localOptimizedDir.mkdirs()) {
            return null;
        }

        return tryCreatingClassLoader(canaryClassName, localDexFile, localOptimizedDir);
    }

    /**
     * Deletes any files cached by {@link #load()}.
     * @param localDexDir Cache directory passed to {@link #load()}.
     */
    public static void deleteCachedDexes(File localDexDir) {
        deleteChildren(localDexDir);
    }

    /**
     * Deletes all of a directory's children including subdirectories.
     * @param parentDir Directory whose children should be deleted.
     */
    private static void deleteChildren(File parentDir) {
        if (!parentDir.isDirectory()) {
            return;
        }

        File[] files = parentDir.listFiles();
        if (files != null) {
            for (File file : files) {
                deleteChildren(file);
                if (!file.delete()) {
                    Log.e(TAG, "Could not delete " + file.getPath());
                }
            }
        }
    }

    /**
     * Copies an InputStream to a file.
     * @param inputStream
     * @param outFile The destination file.
     */
    private static void copyFromInputStream(InputStream inputStream, File outFile) {
        try (FileOutputStream outputStream = new FileOutputStream(outFile)) {
            byte[] buffer = new byte[BUFFER_SIZE];
            int count = 0;
            while ((count = inputStream.read(buffer, 0, BUFFER_SIZE)) != -1) {
                outputStream.write(buffer, 0, count);
            }
        } catch (IOException e) {
        } finally {
            try {
                inputStream.close();
            } catch (Exception e) {
            }
        }
    }

    /**
     * Tries to create ClassLoader with the given .dex file and optimized dex directory.
     * @param canaryClassName Name of class in the .dex file. Used for testing the ClassLoader
     *                        before returning it.
     * @param dexFile .dex file to create ClassLoader for.
     * @param optimizedDir Directory for storing the optimized dex file. If null, an OS-defined
     *                     path based on {@link dexFile} is used.
     * @return The ClassLoader. Returns null on an error.
     */
    private static ClassLoader tryCreatingClassLoader(
            String canaryClassName, File dexFile, File optimizedDir) {
        try {
            ClassLoader loader = new BaseDexClassLoader(
                    dexFile.getPath(), optimizedDir, null, ClassLoader.getSystemClassLoader());
            // Loading {@link canaryClassName} will throw an exception if the .dex file cannot be
            // loaded.
            loader.loadClass(canaryClassName);
            return loader;
        } catch (Exception e) {
            String optimizedDirPath = (optimizedDir == null) ? null : optimizedDir.getPath();
            Log.w(TAG, "Could not load dex from " + dexFile.getPath() + " with optimized directory "
                            + optimizedDirPath);
            e.printStackTrace();
            return null;
        }
    }
}

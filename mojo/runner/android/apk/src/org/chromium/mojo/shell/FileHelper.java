// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.shell;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.util.Log;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Helper methods for file extraction from APK assets and zip archives.
 */
class FileHelper {
    public static final String TAG = "MojoFileHelper";

    // Size of the buffer used in streaming file operations.
    private static final int BUFFER_SIZE = 1024 * 1024;
    // Prefix used when naming temporary files.
    private static final String TEMP_FILE_PREFIX = "temp-";
    // Prefix used when naming timestamp files.
    private static final String TIMESTAMP_PREFIX = "asset_timestamp-";

    /**
     * Looks for a timestamp file on disk that indicates the version of the APK that the resource
     * assets were extracted from. Returns null if a timestamp was found and it indicates that the
     * resources match the current APK. Otherwise returns a String that represents the filename of a
     * timestamp to create.
     */
    private static String checkAssetTimestamp(Context context, File outputDir) {
        PackageManager pm = context.getPackageManager();
        PackageInfo pi = null;

        try {
            pi = pm.getPackageInfo(context.getPackageName(), 0);
        } catch (PackageManager.NameNotFoundException e) {
            return TIMESTAMP_PREFIX;
        }

        if (pi == null) {
            return TIMESTAMP_PREFIX;
        }

        String expectedTimestamp = TIMESTAMP_PREFIX + pi.versionCode + "-" + pi.lastUpdateTime;

        String[] timestamps = outputDir.list(new FilenameFilter() {
            @Override
            public boolean accept(File dir, String name) {
                return name.startsWith(TIMESTAMP_PREFIX);
            }
        });

        if (timestamps.length != 1) {
            // If there's no timestamp, nuke to be safe as we can't tell the age of the files.
            // If there's multiple timestamps, something's gone wrong so nuke.
            return expectedTimestamp;
        }

        if (!expectedTimestamp.equals(timestamps[0])) {
            return expectedTimestamp;
        }

        // Timestamp file is already up-to date.
        return null;
    }

    public static File extractFromAssets(Context context, String assetName, File outputDirectory,
            boolean useTempFile) throws IOException, FileNotFoundException {
        String timestampToCreate = null;
        if (!useTempFile) {
            timestampToCreate = checkAssetTimestamp(context, outputDirectory);
            if (timestampToCreate != null) {
                for (File child : outputDirectory.listFiles()) {
                    deleteRecursively(child);
                }
            }
        }

        File outputFile;
        if (useTempFile) {
            // Make the original filename part of the temp file name.
            // TODO(ppi): do we need to sanitize the suffix?
            String suffix = "-" + assetName;
            outputFile = File.createTempFile(TEMP_FILE_PREFIX, suffix, outputDirectory);
        } else {
            outputFile = new File(outputDirectory, assetName);
            if (outputFile.exists()) {
                return outputFile;
            }
            outputFile.getParentFile().mkdirs();
        }

        BufferedInputStream inputStream = new BufferedInputStream(
                context.getAssets().open(assetName));
        try {
            writeStreamToFile(inputStream, outputFile);
        } finally {
            inputStream.close();
        }

        if (timestampToCreate != null) {
            try {
                new File(outputDirectory, timestampToCreate).createNewFile();
            } catch (IOException e) {
                // In the worst case we don't write a timestamp, so we'll re-extract the asset next
                // time.
                Log.w(TAG, "Failed to write asset timestamp!");
            }
        }

        return outputFile;
    }

    /**
     * Extracts the file of the given extension from the archive. Throws FileNotFoundException if no
     * matching file is found.
     */
    static File extractFromArchive(File archive, String suffixToMatch,
            File outputDirectory) throws IOException, FileNotFoundException {
        ZipInputStream zip = new ZipInputStream(new BufferedInputStream(new FileInputStream(
                archive)));
        ZipEntry entry;
        while ((entry = zip.getNextEntry()) != null) {
            if (entry.getName().endsWith(suffixToMatch)) {
                // Make the original filename part of the temp file name.
                // TODO(ppi): do we need to sanitize the suffix?
                String suffix = "-" + new File(entry.getName()).getName();
                File extractedFile = File.createTempFile(TEMP_FILE_PREFIX, suffix,
                        outputDirectory);
                writeStreamToFile(zip, extractedFile);
                zip.close();
                return extractedFile;
            }
        }
        zip.close();
        throw new FileNotFoundException();
    }

    /**
     * Deletes a file or directory. Directory will be deleted even if not empty.
     */
    static void deleteRecursively(File file) {
        if (file.isDirectory()) {
            for (File child : file.listFiles()) {
                deleteRecursively(child);
            }
        }
        if (!file.delete()) {
            Log.w(TAG, "Unable to delete file: " + file.getAbsolutePath());
        }
    }

    private static void writeStreamToFile(InputStream inputStream, File outputFile)
            throws IOException {
        byte[] buffer = new byte[BUFFER_SIZE];
        OutputStream outputStream = new BufferedOutputStream(new FileOutputStream(outputFile));
        try {
            int read;
            while ((read = inputStream.read(buffer, 0, BUFFER_SIZE)) > 0) {
                outputStream.write(buffer, 0, read);
            }
        } finally {
            outputStream.close();
        }
    }
}

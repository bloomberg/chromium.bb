// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.shell;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;

import org.chromium.base.Log;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.List;
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
    // Name of the file listing assets to extract with one asset file per line.
    private static final String ASSETS_LIST_NAME = "assets_list";
    // Directory where applications cached with the shell will be extracted.
    private static final String CACHED_APP_DIRECTORY = "cached_apps";

    /**
     * Used to indicate the type of destination file that should be created.
     */
    public enum FileType {
        TEMPORARY,
        PERMANENT,
    }

    public enum ArchiveType {
        /**
         * The archive was created for a content handler (contains the mojo escape sequence).
         */
        CONTENT_HANDLER,
        NORMAL,
    }

    /**
     * Looks for a timestamp file on disk that indicates the version of the APK that the resource
     * assets were extracted from. Returns null if a timestamp was found and it indicates that the
     * resources match the current APK. Otherwise returns the file to create.
     */
    private static File findAssetTimestamp(Context context, File outputDir) {
        PackageManager pm = context.getPackageManager();
        PackageInfo pi = null;

        try {
            pi = pm.getPackageInfo(context.getPackageName(), 0);
        } catch (PackageManager.NameNotFoundException e) {
        }

        if (pi == null) {
            return new File(outputDir, TIMESTAMP_PREFIX);
        }

        final File expectedTimestamp =
                new File(outputDir, TIMESTAMP_PREFIX + pi.versionCode + "-" + pi.lastUpdateTime);
        return expectedTimestamp.exists() ? null : expectedTimestamp;
    }

    /**
     * Returns the directory where cached applications will be extracted.
     */
    public static File getCachedAppsDir(Context context) {
        return context.getDir(CACHED_APP_DIRECTORY, Context.MODE_PRIVATE);
    }

    /**
     * Returns the names of the assets in ASSETS_LIST_NAME.
     */
    public static List<String> getAssetsList(Context context) throws IOException {
        List<String> results = new ArrayList<String>();
        BufferedReader reader = new BufferedReader(new InputStreamReader(
                context.getAssets().open(ASSETS_LIST_NAME), Charset.forName("UTF-8")));

        try {
            String line;
            while ((line = reader.readLine()) != null) {
                line = line.trim();
                // These two are read by the system and don't need to be extracted.
                if (!line.isEmpty() && !line.equals("bootstrap_java.dex.jar")
                        && !line.equals("libbootstrap.so")) {
                    results.add(line);
                }
            }
        } finally {
            reader.close();
        }
        return results;
    }

    /**
     * Invoke prior to extracting any assets into {@code directory}. If necessary deletes all the
     * files in the specified directory. The return value must be supplied to {@link
     *createTimestampIfNecessary}.
     *
     * @param directory directory assets will be extracted to
     * @return non-null if a file with the specified name needs to be created after assets have
     * been extracted.
     */
    public static File prepareDirectoryForAssets(Context context, File directory) {
        final File timestamp = findAssetTimestamp(context, directory);
        if (timestamp == null) {
            return null;
        }
        for (File child : directory.listFiles()) {
            deleteRecursively(child);
        }
        return timestamp;
    }

    /**
     * Creates a file used as a timestamp. The supplied file comes from {@link
     *prepareDirectoryForAssets}.
     *
     * @param timestamp path of file to create, or null if a file does not need to be created
     */
    public static void createTimestampIfNecessary(File timestamp) {
        if (timestamp == null) {
            return;
        }
        try {
            timestamp.createNewFile();
        } catch (IOException e) {
            // In the worst case we don't write a timestamp, so we'll re-extract the asset next
            // time.
            Log.w(TAG, "Failed to write asset timestamp!");
        }
    }

    public static File extractFromAssets(Context context, String assetName, File outputDirectory,
            FileType fileType) throws IOException, FileNotFoundException {
        File outputFile;
        if (fileType == FileType.TEMPORARY) {
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

        BufferedInputStream inputStream =
                new BufferedInputStream(context.getAssets().open(assetName));
        try {
            writeStreamToFile(inputStream, outputFile);
        } finally {
            inputStream.close();
        }
        return outputFile;
    }

    /**
     * Extracts the file of the given extension from the archive. Throws FileNotFoundException if no
     * matching file is found.
     *
     * @return path of extracted file
     */
    static File extractFromArchive(File archive, String suffixToMatch, File outputDirectory,
            FileType fileType, ArchiveType archiveType) throws IOException, FileNotFoundException {
        if (!outputDirectory.exists() && !outputDirectory.mkdirs()) {
            Log.e(TAG, "extractFromArchive unable to create directory "
                            + outputDirectory.getAbsolutePath());
            throw new FileNotFoundException();
        }

        BufferedInputStream inputStream = new BufferedInputStream(new FileInputStream(archive));
        if (archiveType == ArchiveType.CONTENT_HANDLER) {
            int currentChar;
            do {
                currentChar = inputStream.read();
            } while (currentChar != -1 && currentChar != '\n');
            if (currentChar == -1) {
                throw new FileNotFoundException();
            }
            inputStream = new BufferedInputStream(inputStream);
        }
        ZipInputStream zip = new ZipInputStream(inputStream);
        ZipEntry entry;
        while ((entry = zip.getNextEntry()) != null) {
            if (entry.getName().endsWith(suffixToMatch)) {
                // TODO(sky): sanitize name.
                final String name = new File(entry.getName()).getName();
                File extractedFile;
                // Make the original filename part of the temp file name.
                if (fileType == FileType.TEMPORARY) {
                    final String suffix = "-" + name;
                    extractedFile = File.createTempFile(TEMP_FILE_PREFIX, suffix, outputDirectory);
                } else {
                    extractedFile = new File(outputDirectory, name);
                }
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

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.variations;

import android.os.ParcelFileDescriptor;

import org.chromium.base.CollectionUtil;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher.SeedInfo;

import java.io.BufferedReader;
import java.io.Closeable;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.TimeUnit;

/**
 * Constants and shared methods for fetching and storing the WebView variations seed.
 */
public class AwVariationsUtils {
    private static final String TAG = "AwVariationsUtils";

    // Types for the WebViews to identify the message. If it is MSG_WITH_SEED_DATA, the WebView will
    // store the seed in the message to the app directory.
    static final int MSG_WITHOUT_SEED_DATA = 0;
    static final int MSG_WITH_SEED_DATA = 1;

    // Types for the service to identify the message which Context binds to it.
    static final int MSG_SEED_REQUEST = 2;
    static final int MSG_SEED_TRANSFER = 3;

    // The file names used to store the variations seed.
    public static final String SEED_DATA_FILENAME = "variations_seed_data";
    public static final String SEED_PREF_FILENAME = "variations_seed_pref";

    // The key string used in the message to store the variations seed.
    static final String KEY_SEED_DATA = "KEY_SEED_DATA";
    static final String KEY_SEED_PREF = "KEY_SEED_PREF";

    // The expiration time for the Finch seed.
    static final long SEED_EXPIRATION_TIME_IN_MILLIS = TimeUnit.MINUTES.toMillis(30);

    // Buffer size when copying seed from service directory to app directory.
    static final int BUFFER_SIZE = 4096;

    private static void copyBytes(FileInputStream in, FileOutputStream out) throws IOException {
        byte[] buffer = new byte[BUFFER_SIZE];
        int readCount = 0;
        while ((readCount = in.read(buffer)) > 0) {
            out.write(buffer, 0, readCount);
        }
    }

    /**
     * Copy the file in the parcel file descriptor to the variations directory in the app directory.
     * @param fileDescriptor The file descriptor of an opened file.
     * @param fileName The name for the destination file in the variations directory.
     * @throws IOException if fail to get the directory or copy file to the aoo
     */
    @VisibleForTesting
    public static void copyFileToVariationsDirectory(ParcelFileDescriptor fileDescriptor,
            File variationsDir, String fileName) throws IOException {
        if (fileDescriptor == null) {
            Log.e(TAG, "Variations seed file descriptor is null.");
            return;
        }
        FileInputStream in = null;
        FileOutputStream out = null;
        File tempFile = null;
        try {
            tempFile = File.createTempFile(fileName, null, variationsDir);
            if (!tempFile.setWritable(false /* writable */, false /* ownerOnly */)
                    || !tempFile.setWritable(true /* writable */, true /* ownerOnly */)) {
                Log.e(TAG, "Failed to set write permissions for temporary file. ");
            }
            in = new FileInputStream(fileDescriptor.getFileDescriptor());
            out = new FileOutputStream(tempFile);
            copyBytes(in, out);
        } finally {
            closeStream(in);
            closeStream(out);
            try {
                fileDescriptor.close();
            } catch (IOException e) {
                Log.e(TAG, "Failed to close seed data file descriptor. " + e);
            }
        }
        renameTempFile(tempFile, new File(variationsDir, fileName));
    }

    /**
     * Rename a temp file to a new name.
     * @param tempFile The temp file.
     * @param newFile The new file.
     */
    public static void renameTempFile(File tempFile, File newFile) {
        // TODO(paulmiller): Fix running on UI thread.
        ThreadUtils.setThreadAssertsDisabledForTesting(true);
        try {
            FileUtils.recursivelyDeleteFile(newFile);
        } finally {
            ThreadUtils.setThreadAssertsDisabledForTesting(false);
        }
        boolean isTempFileRenamed = tempFile.renameTo(newFile);
        if (!isTempFileRenamed) {
            Log.e(TAG,
                    "Failed to rename " + tempFile.getAbsolutePath() + " to "
                            + newFile.getAbsolutePath() + ".");
        }
    }

    /**
     * Safely close a stream.
     * @param stream The stream to be closed.
     */
    public static void closeStream(Closeable stream) {
        if (stream != null) {
            try {
                stream.close();
            } catch (IOException e) {
                Log.e(TAG, "Failed to close stream. " + e);
            }
        }
    }

    /**
     * SeedPreference is used to serialize/deserialize related fields of seed data when reading or
     * writing them into file.
     */
    public static class SeedPreference {
        // VariationsSeedFetcher shared between Chromium and WebView returns a gzip field that is
        // unused by WebView.
        public static final int FIELD_COUNT = 4;

        public final String signature;
        public final String country;
        public final String date;

        private SeedPreference(List<String> seedPrefAsList) {
            signature = seedPrefAsList.get(0);
            country = seedPrefAsList.get(1);
            date = seedPrefAsList.get(2);
        }

        public SeedPreference(SeedInfo seedInfo) {
            signature = seedInfo.signature;
            country = seedInfo.country;
            date = seedInfo.date;
        }

        public ArrayList<String> toArrayList() {
            // Dummy field so that calls to toArrayList and fromList won't fail the FIELD_COUNT
            // check.
            String gzipDummy = "";
            return CollectionUtil.newArrayList(signature, country, date, gzipDummy);
        }

        public static SeedPreference fromList(List<String> seedPrefAsList) {
            if (seedPrefAsList.size() != FIELD_COUNT) {
                Log.e(TAG, "Failed to validate the seed preference, field count is incorrect).");
                return null;
            }
            return new SeedPreference(seedPrefAsList);
        }

        /**
         * Judge if the current Finch seed in a given directory is expired.
         * @return True if the seed is expired or missing, otherwise false.
         */
        public boolean seedNeedsUpdate() {
            Date lastSeedFetchTime = parseDateTimeString(date);
            if (lastSeedFetchTime == null) return true;
            return new Date().getTime() - lastSeedFetchTime.getTime()
                    > SEED_EXPIRATION_TIME_IN_MILLIS;
        }
    }

    /**
     * Get the variations seed preference in a given directory.
     * @return The seed preference.
     * @throws IOException if fail to read the seed preference file in the given directory.
     */
    @VisibleForTesting
    public static SeedPreference readSeedPreference(File variationsDir) throws IOException {
        BufferedReader reader = null;
        File seedPrefFile = new File(variationsDir, AwVariationsUtils.SEED_PREF_FILENAME);
        try {
            reader = new BufferedReader(new FileReader(seedPrefFile));
            ArrayList<String> seedPrefAsList = new ArrayList<String>();
            String line = null;
            while ((line = reader.readLine()) != null) {
                seedPrefAsList.add(line);
            }
            return AwVariationsUtils.SeedPreference.fromList(seedPrefAsList);
        } finally {
            closeStream(reader);
        }
    }

    /**
     * Parse the string containing date time info to a Date object.
     * @param dateTimeStr The string holds the date time info.
     * @return The Date object holds the date info from the input string, null if parse failed.
     */
    private static Date parseDateTimeString(String dateTimeStr) {
        Date lastSeedFetchTime = null;
        try {
            final SimpleDateFormat dateFormat =
                    new SimpleDateFormat("EEE, dd MMM yyyy HH:mm:ss z", Locale.US);
            lastSeedFetchTime = dateFormat.parse(dateTimeStr);
        } catch (ParseException e) {
            Log.e(TAG, "Failed to parse the time. " + e);
        }
        return lastSeedFetchTime;
    }
}

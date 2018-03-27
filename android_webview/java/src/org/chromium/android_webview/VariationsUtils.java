// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.support.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher.SeedInfo;

import java.io.Closeable;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Date;

/**
 * Utilities for manipulating variations seeds, used by both WebView and WebView's services.
 */
public class VariationsUtils {
    private static final String TAG = "VariationsUtils";

    private static final String SEED_FILE_NAME = "variations_seed";
    private static final String NEW_SEED_FILE_NAME = "variations_seed_new";
    private static final String STAMP_FILE_NAME = "variations_stamp";

    private static void closeSafely(Closeable c) {
        if (c != null) {
            try {
                c.close();
            } catch (IOException e) {
                Log.e(TAG, "Failed to close " + c);
            }
        }
    }

    // Both the WebView variations service and apps using WebView keep a pair of seed files in their
    // data directory. New seeds are written to the new seed file, and then the old file is replaced
    // with the new file.
    public static File getSeedFile() {
        return new File(PathUtils.getDataDirectory(), SEED_FILE_NAME);
    }

    public static File getNewSeedFile() {
        return new File(PathUtils.getDataDirectory(), NEW_SEED_FILE_NAME);
    }

    public static void replaceOldWithNewSeed() {
        File oldSeedFile = getSeedFile();
        File newSeedFile = getNewSeedFile();
        if (!newSeedFile.renameTo(oldSeedFile)) {
            Log.e(TAG, "Failed to replace old seed " + oldSeedFile +
                    " with new seed " + newSeedFile);
        }
    }

    // There's a 3rd timestamp file whose modification time is the time of the last seed request. In
    // the app, this is used to rate-limit seed requests. In the service, this is used to cancel the
    // periodic seed fetch if no app requests the seed for a long time.
    public static File getStampFile() {
        return new File(PathUtils.getDataDirectory(), STAMP_FILE_NAME);
    }

    // Get the timestamp, in milliseconds since epoch, or 0 if the file doesn't exist.
    public static long getStampTime() {
        return getStampFile().lastModified();
    }

    // Creates/updates the timestamp with the current time.
    public static void updateStampTime() {
        File file = getStampFile();
        try {
            if (!file.createNewFile()) {
                long now = (new Date()).getTime();
                file.setLastModified(now);
            }
        } catch (IOException e) {
            Log.e(TAG, "Failed to write " + file);
        }
    }

    // Returns null in case of incomplete/corrupt/missing seed.
    @Nullable
    public static SeedInfo readSeedFile(File inFile) {
        // Read and discard the seed file, then return a mock seed. TODO(paulmiller): Return the
        // actual seed, once seed downloading and serialization are implemented.
        FileInputStream in = null;
        try {
            in = new FileInputStream(inFile);
            byte[] data = new byte[1024];
            while (in.read(data) > 0) {}

            SeedInfo seed = new SeedInfo();
            // Fill in a mock date so that seed.parseDate() doesn't crash.
            seed.date = "Thu, 01 Jan 1970 12:34:56 GMT";
            return seed;
        } catch (IOException e) {
            Log.e(TAG, "Failed reading seed file \"" + inFile + "\": " + e.getMessage());
            return null;
        } finally {
            closeSafely(in);
        }
    }

    // Returns true on success. "out" will always be closed, regardless of success.
    public static boolean writeSeed(FileOutputStream out, SeedInfo info) {
        // Write 3 KB of zeros (the current size of an actual WebView seed). TODO(paulmiller): Write
        // the actual seed, once seed downloading and serialization are implemented.
        byte[] zeros = new byte[3 * 1024];
        try {
            out.write(zeros);
            return true;
        } catch (IOException e) {
            Log.e(TAG, "Failed writing seed file: " + e.getMessage());
            return false;
        } finally {
            closeSafely(out);
        }
    }
}

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.text.TextUtils;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;

import java.util.UUID;

/**
 * Class representing the download information stored in SharedPreferences to construct a
 * download notification.
 */
public class DownloadSharedPreferenceEntry {
    private static final String TAG = "DownloadEntry";
    // Current version of the DownloadSharedPreferenceEntry. When changing the SharedPreference,
    // we need to change the version number too.
    @VisibleForTesting static final int VERSION = 1;
    public final int notificationId;
    public final boolean isResumable;
    public boolean canDownloadWhileMetered;
    public final String fileName;
    public final String downloadGuid;

    DownloadSharedPreferenceEntry(int notificationId, boolean isResumable,
            boolean canDownloadWhileMetered, String guid, String fileName) {
        this.notificationId = notificationId;
        this.isResumable = isResumable;
        this.canDownloadWhileMetered = canDownloadWhileMetered;
        this.downloadGuid = guid;
        this.fileName = fileName;
    }

    /**
     * Parse the pending notification from a String object in SharedPrefs.
     *
     * @param sharedPrefString String from SharedPreference, containing the notification ID, GUID,
     *        file name, whether it is resumable and whether download started on a metered network.
     * @return a DownloadSharedPreferenceEntry object.
     */
    static DownloadSharedPreferenceEntry parseFromString(String sharedPrefString) {
        String[] values = sharedPrefString.split(",", 6);
        if (values.length == 6) {
            try {
                int version = Integer.parseInt(values[0]);
                // Ignore all SharedPreference entries that has an invalid version for now.
                if (version != VERSION) {
                    return new DownloadSharedPreferenceEntry(-1, false, false, null, "");
                }
                int id = Integer.parseInt(values[1]);
                boolean isResumable = "1".equals(values[2]);
                boolean canDownloadWhileMetered = "1".equals(values[3]);
                if (!isValidGUID(values[4])) {
                    return new DownloadSharedPreferenceEntry(-1, false, false, null, "");
                }
                return new DownloadSharedPreferenceEntry(
                        id, isResumable, canDownloadWhileMetered, values[4], values[5]);
            } catch (NumberFormatException nfe) {
                Log.w(TAG, "Exception while parsing pending download:" + sharedPrefString);
            }
        }
        return new DownloadSharedPreferenceEntry(-1, false, false, null, "");
    }

    /**
     * @return a string for the DownloadSharedPreferenceEntry instance to be inserted into
     *         SharedPrefs.
     */
    String getSharedPreferenceString() {
        return VERSION + "," + notificationId + "," + (isResumable ? "1" : "0") + ","
                + (canDownloadWhileMetered ? "1" : "0") + "," + downloadGuid + "," + fileName;
    }

    /**
     * Check if a string is a valid GUID. GUID is RFC 4122 compliant, it should have format
     * xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx.
     * TODO(qinmin): move this to base/.
     * @return true if the string is a valid GUID, or false otherwise.
     */
    static boolean isValidGUID(String guid) {
        if (guid == null) return false;
        try {
            // Java UUID class doesn't check the length of the string. Need to convert it back to
            // string so that we can validate the length of the original string.
            UUID uuid = UUID.fromString(guid);
            String uuidString = uuid.toString();
            return guid.equalsIgnoreCase(uuidString);
        } catch (IllegalArgumentException e) {
            return false;
        }
    }

    /**
     * Build a download item from this object.
     */
    DownloadItem buildDownloadItem() {
        DownloadInfo info = new DownloadInfo.Builder()
                .setDownloadGuid(downloadGuid)
                .setFileName(fileName)
                .setIsResumable(isResumable)
                .build();
        return new DownloadItem(false, info);
    }

    @Override
    public boolean equals(Object object) {
        if (!(object instanceof DownloadSharedPreferenceEntry)) {
            return false;
        }
        final DownloadSharedPreferenceEntry other = (DownloadSharedPreferenceEntry) object;
        return TextUtils.equals(downloadGuid, other.downloadGuid)
                && TextUtils.equals(fileName, other.fileName)
                && notificationId == other.notificationId
                && isResumable == other.isResumable
                && canDownloadWhileMetered == other.canDownloadWhileMetered;
    }

    @Override
    public int hashCode() {
        int hash = 31;
        hash = 37 * hash + (isResumable ? 1 : 0);
        hash = 37 * hash + (canDownloadWhileMetered ? 1 : 0);
        hash = 37 * hash + notificationId;
        hash = 37 * hash + downloadGuid.hashCode();
        hash = 37 * hash + fileName.hashCode();
        return hash;
    }
}

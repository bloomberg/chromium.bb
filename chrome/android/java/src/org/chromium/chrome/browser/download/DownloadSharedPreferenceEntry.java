// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

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
    // This field is not yet used, but will soon be used. We add it here to avoid changing the
    // format of the SharedPreference string again.
    public final boolean isStartedOnMeteredNetwork;
    public final String fileName;
    public final String downloadGuid;

    DownloadSharedPreferenceEntry(int notificationId, boolean isResumable,
            boolean isStartedOnMeteredNetwork, String guid, String fileName) {
        this.notificationId = notificationId;
        this.isResumable = isResumable;
        this.isStartedOnMeteredNetwork = isStartedOnMeteredNetwork;
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
                boolean isStartedOnMeteredNetwork = "1".equals(values[3]);
                if (!isValidGUID(values[4])) {
                    return new DownloadSharedPreferenceEntry(-1, false, false, null, "");
                }
                return new DownloadSharedPreferenceEntry(
                        id, isResumable, isStartedOnMeteredNetwork, values[4], values[5]);
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
                + (isStartedOnMeteredNetwork ? "1" : "0") + "," + downloadGuid + "," + fileName;
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
}

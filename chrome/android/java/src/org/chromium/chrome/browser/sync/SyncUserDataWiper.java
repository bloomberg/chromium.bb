// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import org.chromium.chrome.browser.BrowsingDataType;
import org.chromium.chrome.browser.TimePeriod;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PrefServiceBridge.OnClearBrowsingDataListener;

/**
 * A class to wipe the user's bookmarks and all types of sync data.
 */
public class SyncUserDataWiper {
    private static final int[] SYNC_DATA_TYPES = {
            BrowsingDataType.HISTORY,
            BrowsingDataType.CACHE,
            BrowsingDataType.COOKIES,
            BrowsingDataType.PASSWORDS,
            BrowsingDataType.FORM_DATA
    };

    /**
     * Wipes the user's bookmarks and sync data.
     * @param callback Called when the data is cleared.
     */
    public static void wipeSyncUserData(final Runnable callback) {
        final BookmarkModel model = new BookmarkModel();

        model.runAfterBookmarkModelLoaded(new Runnable() {
            @Override
            public void run() {
                model.removeAllUserBookmarks();
                model.destroy();
                PrefServiceBridge.getInstance().clearBrowsingData(
                        new OnClearBrowsingDataListener(){
                            @Override
                            public void onBrowsingDataCleared() {
                                callback.run();
                            }
                        },
                        SYNC_DATA_TYPES, TimePeriod.EVERYTHING);
            }
        });
    }
}


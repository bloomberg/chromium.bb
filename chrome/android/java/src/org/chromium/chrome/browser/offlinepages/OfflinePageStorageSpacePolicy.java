// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import org.chromium.base.Callback;

import java.util.List;

/**
 * Manages the storage space policy for offline pages.
 *
 * When created, it records the size on disk that is being used, this value never changes after
 * creation so make a new OfflinePageStorageSpacePolicy whenever a new measurement is desired.
 */
public class OfflinePageStorageSpacePolicy {
    /**
     * Minimal total size of all pages, before a header will be shown to offer freeing up space.
     */
    private static final long MINIMUM_TOTAL_SIZE_BYTES = 10 * (1 << 20); // 10MB
    /**
     * Minimal size of pages to clean up, before a header will be shown to offer freeing up space.
     */
    private static final long MINIMUM_CLEANUP_SIZE_BYTES = 5 * (1 << 20); // 5MB

    private long mSizeOfAllPages;
    private long mSizeOfPagesToCleanUp;

    /**
     * Asynchronously creates an OffinePageStorageSpacePolicy which is prefilled with information
     * about the state of the disk usage of Offline Pages.
     */
    public static void create(final OfflinePageBridge offlinePageBridge,
            final Callback<OfflinePageStorageSpacePolicy> callback) {
        assert offlinePageBridge != null;
        offlinePageBridge.getAllPages(new OfflinePageBridge.MultipleOfflinePageItemCallback() {
            @Override
            public void onResult(List<OfflinePageItem> allPages) {
                callback.onResult(new OfflinePageStorageSpacePolicy(offlinePageBridge, allPages));
            }
        });
    }

    /**
     * Creates a policy object with the given list of offline pages.
     *
     * @param offlinePageBridge An object to access offline page functionality.
     * @param offlinePages The list of all offline pages.
     */
    private OfflinePageStorageSpacePolicy(
            OfflinePageBridge offlinePageBridge, List<OfflinePageItem> offlinePages) {
        mSizeOfAllPages = getTotalSize(offlinePages);
        mSizeOfPagesToCleanUp = getTotalSize(offlinePageBridge.getPagesToCleanUp());
    }

    /** @return Whether there exists offline pages that could be cleaned up to make space. */
    public boolean hasPagesToCleanUp() {
        return mSizeOfPagesToCleanUp > 0;
    }

    /**
     * @return Whether the header should be shown in saved pages view to inform user of total used
     * storage and offer freeing up space.
     */
    public boolean shouldShowStorageSpaceHeader() {
        return getSizeOfAllPages() > MINIMUM_TOTAL_SIZE_BYTES
                && getSizeOfPagesToCleanUp() > MINIMUM_CLEANUP_SIZE_BYTES;
    }

    /** @return Total size, in bytes, of all saved pages. */
    public long getSizeOfAllPages() {
        return mSizeOfAllPages;
    }

    private long getSizeOfPagesToCleanUp() {
        return mSizeOfPagesToCleanUp;
    }

    private long getTotalSize(List<OfflinePageItem> offlinePages) {
        long totalSize = 0;
        for (OfflinePageItem page : offlinePages) {
            totalSize += page.getFileSize();
        }
        return totalSize;
    }
}

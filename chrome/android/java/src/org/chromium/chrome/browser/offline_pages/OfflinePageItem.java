// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offline_pages;

/**
 * Simple object representing an offline page.
 */
public class OfflinePageItem {
    private final String mUrl;
    private final String mTitle;
    private final String mOfflineUrl;
    private final long mFileSize;

    public OfflinePageItem(String url, String title, String offlineUrl, long fileSize) {
        mUrl = url;
        mTitle = title;
        mOfflineUrl = offlineUrl;
        mFileSize = fileSize;
    }

    /** @return URL of the offline page. */
    public String getUrl() {
        return mUrl;
    }

    /** @return Title of the offline page. */
    public String getTitle() {
        return mTitle;
    }

    /** @return Path to the offline copy of the page. */
    public String getOfflineUrl() {
        return mOfflineUrl;
    }

    /** @return Size of the offline copy of the page. */
    public long getFileSize() {
        return mFileSize;
    }
}

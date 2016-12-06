// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.text.TextUtils;

import org.chromium.chrome.browser.widget.DateDividedAdapter.TimedItem;

/** Contains information about a single browsing history item. */
public class HistoryItem extends TimedItem {
    private final String mUrl;
    private final String mDomain;
    private final String mTitle;
    private final long mTimestamp;
    private Long mStableId;

    /**
     * @param url The url for this item.
     * @param domain The string to display for the item's domain.
     * @param title The string to display for the item's title.
     * @param timestamp The timestamp for this item.
     */
    public HistoryItem(String url, String domain, String title, long timestamp) {
        mUrl = url;
        mDomain = domain;
        mTitle = TextUtils.isEmpty(title) ? url : title;
        mTimestamp = timestamp;
    }

    /** @return The string to display for the item's domain. */
    public String getDomain() {
        return mDomain;
    }

    /** @return The string to display for the item's title. */
    public String getTitle() {
        return mTitle;
    }

    @Override
    public long getTimestamp() {
        return mTimestamp;
    }

    @Override
    public long getStableId() {
        if (mStableId == null) {
            // Generate a stable ID that combines the timestamp and the URL.
            mStableId = (long) mUrl.hashCode();
            mStableId = (mStableId << 32) + (getTimestamp() & 0x0FFFFFFFF);
        }
        return mStableId;
    }
}

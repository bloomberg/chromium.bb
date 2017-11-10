// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnerbookmarks;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;

import org.chromium.base.VisibleForTesting;

import java.util.Map;

/**
 * A cache for storing failed favicon loads along with a timestamp determining what point we can
 * attempt retrieval again, as a way of throttling the number of requests made for previously failed
 * favicon fetch attempts.
 */
public class PartnerBookmarksCache {
    private static final String PREFERENCES_NAME = "partner_bookmarks_favicon_cache";

    private final SharedPreferences mSharedPreferences;

    public PartnerBookmarksCache(Context context) {
        this(context, PREFERENCES_NAME);
    }

    @VisibleForTesting
    PartnerBookmarksCache(Context context, String cacheName) {
        mSharedPreferences = context.getSharedPreferences(cacheName, 0);
    }

    /**
     * Reads the favicon retrieval timestamp information from a cache-specific
     * {@link SharedPreferences}.
     *
     * Suppressing "unchecked" because we're 100% sure we're storing only <String, Long> pairs in
     * our cache.
     *
     * @return A map with the favicon URLs that failed retrieval and a timestamp after which we
     *         can retry again.
     */
    @SuppressWarnings("unchecked")
    public Map<String, Long> read() {
        return (Map<String, Long>) mSharedPreferences.getAll();
    }

    /**
     * Writes the provided map into the cache's {@link SharedPreferences}.
     * This overwrites what was previously in the cache.
     *
     * @param inMap The favicon/timestamps to write to cache.
     */
    public void write(Map<String, Long> inMap) {
        if (inMap == null) {
            throw new IllegalArgumentException("PartnerBookmarksCache: write() "
                    + "input cannot be null.");
        }

        Editor editor = mSharedPreferences.edit();
        editor.clear();
        for (Map.Entry<String, Long> entry : inMap.entrySet()) {
            editor.putLong(entry.getKey(), entry.getValue());
        }
        editor.apply();
    }

    /**
     * Called after tests so we don't leave behind test {@link SharedPreferences}, and have data
     * from one test run into another.
     */
    @VisibleForTesting
    void clearCache() {
        mSharedPreferences.edit().clear().apply();
    }
}
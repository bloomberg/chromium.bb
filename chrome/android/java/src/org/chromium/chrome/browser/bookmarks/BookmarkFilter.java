// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

/**
 * Possible filters for the enhanced bookmarks.
 */
enum BookmarkFilter {
    OFFLINE_PAGES("OFFLINE_PAGES");

    /**
     * An {@link BookmarkFilter} can be persisted in URLs. To ensure the
     * URLs are consistent, values should remain the same even after the enums
     * are renamed.
     */
    public final String value;

    private BookmarkFilter(String value) {
        this.value = value;
    }
}

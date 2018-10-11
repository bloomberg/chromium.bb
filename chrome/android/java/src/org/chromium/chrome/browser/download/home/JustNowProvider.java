// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;

import java.util.Date;

/** Helper class to expose whether an item should be shown in the Just Now section. */
public class JustNowProvider {
    // Threshold time interval during which a download is considered recent.
    private static final long JUST_NOW_DEFAULT_THRESHOLD_MS = 30 * 60 * 1000;

    // Threshold timestamp after which a download is considered recent.
    private final Date mThresholdDate;

    /** Constructor. */
    public JustNowProvider() {
        mThresholdDate = new Date(new Date().getTime() - JUST_NOW_DEFAULT_THRESHOLD_MS);
    }

    /**
     * @return Whether the given {@code item} should be shown in the Just Now section.
     */
    public boolean isJustNowItem(OfflineItem item) {
        return item.state == OfflineItemState.IN_PROGRESS || item.state == OfflineItemState.PAUSED
                || (item.state == OfflineItemState.INTERRUPTED && item.isResumable)
                || new Date(item.completionTimeMs).after(mThresholdDate);
    }
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import android.text.TextUtils;

/**
 * Exposes information about the current media to the external clients.
 */
public class MediaInfo {
    /**
     * The title of the media.
     */
    public final String title;

    /**
     * The current state of the media, paused or not.
     */
    public boolean isPaused;

    /**
     * The origin of the tab containing the media.
     */
    public final String origin;

    /**
     * The id of the tab containing the media.
     */
    public final int tabId;

    /**
     * The listener for the control events.
     */
    public final MediaPlaybackListener listener;

    /**
     * Create a new MediaInfo.
     * @param title
     * @param state
     * @param origin
     * @param tabId
     * @param listener
     */
    public MediaInfo(
            String title,
            boolean isPaused,
            String origin,
            int tabId,
            MediaPlaybackListener listener) {
        this.title = title;
        this.isPaused = isPaused;
        this.origin = origin;
        this.tabId = tabId;
        this.listener = listener;
    }

    /**
     * Copy a media info.
     * @param other the source to copy from.
     */
    public MediaInfo(MediaInfo other) {
        this(other.title, other.isPaused, other.origin, other.tabId, other.listener);
    }

    @Override
    public boolean equals(Object obj) {
        if (obj == this) return true;
        if (!(obj instanceof MediaInfo)) return false;

        MediaInfo other = (MediaInfo) obj;
        return isPaused == other.isPaused
                && tabId == other.tabId
                && TextUtils.equals(title, other.title)
                && TextUtils.equals(origin, other.origin)
                && listener.equals(other.listener);
    }

    @Override
    public int hashCode() {
        int result = isPaused ? 1 : 0;
        result = 31 * result + (title == null ? 0 : title.hashCode());
        result = 31 * result + (origin == null ? 0 : origin.hashCode());
        result = 31 * result + tabId;
        result = 31 * result + listener.hashCode();
        return result;
    }
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import android.text.TextUtils;

import org.chromium.chrome.browser.tab.Tab;

/**
 * Exposes information about the current media notification to the external clients.
 */
public class MediaNotificationInfo {
    /**
     * Use this class to construct an instance of {@link MediaNotificationInfo}.
     */
    public static final class Builder {
        private String mTitle = "";
        private boolean mIsPaused = false;
        private String mOrigin = "";
        private int mTabId = Tab.INVALID_TAB_ID;
        private boolean mIsPrivate = true;
        private int mIcon = -1;
        private MediaNotificationListener mListener = null;

        /**
         * Initializes the builder with the default values.
         */
        public Builder() {
        }

        public MediaNotificationInfo build() {
            assert mTitle != null;
            assert mOrigin != null;
            assert mListener != null;

            return new MediaNotificationInfo(
                    mTitle,
                    mIsPaused,
                    mOrigin,
                    mTabId,
                    mIsPrivate,
                    mIcon,
                    mListener);
        }

        public Builder setTitle(String title) {
            mTitle = title;
            return this;
        }

        public Builder setPaused(boolean isPaused) {
            mIsPaused = isPaused;
            return this;
        }

        public Builder setOrigin(String origin) {
            mOrigin = origin;
            return this;
        }

        public Builder setTabId(int tabId) {
            mTabId = tabId;
            return this;
        }

        public Builder setPrivate(boolean isPrivate) {
            mIsPrivate = isPrivate;
            return this;
        }

        public Builder setIcon(int icon) {
            mIcon = icon;
            return this;
        }

        public Builder setListener(MediaNotificationListener listener) {
            mListener = listener;
            return this;
        }
    }

    /**
     * The title of the media.
     */
    public final String title;

    /**
     * The current state of the media, paused or not.
     */
    public final boolean isPaused;

    /**
     * The origin of the tab containing the media.
     */
    public final String origin;

    /**
     * The id of the tab containing the media.
     */
    public final int tabId;

    /**
     * Whether the media notification should be considered as private.
     */
    public final boolean isPrivate;

    /**
     * The id of the notification icon from R.drawable.
     */
    public final int icon;

    /**
     * The listener for the control events.
     */
    public final MediaNotificationListener listener;

    /**
     * Create a new MediaNotificationInfo.
     * @param title The title of the media.
     * @param isPaused The current state of the media, paused or not.
     * @param origin The origin of the tab containing the media.
     * @param tabId The id of the tab containing the media.
     * @param isPrivate Whether the media notification should be considered as private.
     * @param listener The listener for the control events.
     */
    private MediaNotificationInfo(
            String title,
            boolean isPaused,
            String origin,
            int tabId,
            boolean isPrivate,
            int icon,
            MediaNotificationListener listener) {
        this.title = title;
        this.isPaused = isPaused;
        this.origin = origin;
        this.tabId = tabId;
        this.isPrivate = isPrivate;
        this.icon = icon;
        this.listener = listener;
    }

    @Override
    public boolean equals(Object obj) {
        if (obj == this) return true;
        if (!(obj instanceof MediaNotificationInfo)) return false;

        MediaNotificationInfo other = (MediaNotificationInfo) obj;
        return isPaused == other.isPaused
                && isPrivate == other.isPrivate
                && tabId == other.tabId
                && icon == other.icon
                && TextUtils.equals(title, other.title)
                && TextUtils.equals(origin, other.origin)
                && listener.equals(other.listener);
    }

    @Override
    public int hashCode() {
        int result = isPaused ? 1 : 0;
        result = 31 * result + (isPrivate ? 1 : 0);
        result = 31 * result + (title == null ? 0 : title.hashCode());
        result = 31 * result + (origin == null ? 0 : origin.hashCode());
        result = 31 * result + tabId;
        result = 31 * result + icon;
        result = 31 * result + listener.hashCode();
        return result;
    }
}

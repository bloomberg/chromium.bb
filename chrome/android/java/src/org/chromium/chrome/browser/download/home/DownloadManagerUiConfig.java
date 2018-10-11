// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import org.chromium.chrome.browser.ChromeFeatureList;

/** Provides the configuration params required by the download home UI. */
public class DownloadManagerUiConfig {
    /** Whether or not the UI should include off the record items. */
    public final boolean isOffTheRecord;

    /** Whether or not the UI should be shown as part of a separate activity. */
    public final boolean isSeparateActivity;

    /**
     * The time interval during which a download update is considered recent enough to show
     * in Just Now section.
     */
    public final long justNowThresholdSeconds;

    /** Constructor. */
    private DownloadManagerUiConfig(Builder builder) {
        isOffTheRecord = builder.mIsOffTheRecord;
        isSeparateActivity = builder.mIsSeparateActivity;
        justNowThresholdSeconds = builder.mJustNowThresholdSeconds;
    }

    /** Helper class for building a {@link DownloadManagerUiConfig}. */
    public static class Builder {
        private static final String JUST_NOW_THRESHOLD_SECONDS_PARAM = "just_now_threshold";

        /** Default value for threshold time interval to show up in Just Now section. */
        private static final int JUST_NOW_THRESHOLD_SECONDS_DEFAULT = 30 * 60;

        private boolean mIsOffTheRecord;
        private boolean mIsSeparateActivity;
        private long mJustNowThresholdSeconds;

        public Builder() {
            readParamsFromFinch();
        }

        public Builder setIsOffTheRecord(boolean isOffTheRecord) {
            mIsOffTheRecord = isOffTheRecord;
            return this;
        }

        public Builder setIsSeparateActivity(boolean isSeparateActivity) {
            mIsSeparateActivity = isSeparateActivity;
            return this;
        }

        public DownloadManagerUiConfig build() {
            return new DownloadManagerUiConfig(this);
        }

        private void readParamsFromFinch() {
            mJustNowThresholdSeconds = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.DOWNLOAD_HOME_V2, JUST_NOW_THRESHOLD_SECONDS_PARAM,
                    JUST_NOW_THRESHOLD_SECONDS_DEFAULT);
        }
    }
}

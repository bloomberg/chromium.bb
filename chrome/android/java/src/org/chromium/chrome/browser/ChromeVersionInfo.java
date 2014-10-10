// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;

import org.chromium.base.BuildInfo;

/**
 * A utility class for determining information about the current Chrome build.
 * Intentionally doesn't depend on native so that the data can be accessed before
 * libchrome.so is loaded.
 */
public class ChromeVersionInfo {
    /** Local builds. */
    private static final int CHANNEL_UNKNOWN = 0x1;

    /** Canary builds. */
    private static final int CHANNEL_CANARY = 0x10;

    /** Dev builds. */
    private static final int CHANNEL_DEV = 0x100;

    /** Beta builds. */
    private static final int CHANNEL_BETA = 0x1000;

    /** Stable builds. */
    private static final int CHANNEL_STABLE = 0x10000;

    /** Signifies that init() hasn't been called yet. */
    private static final int INVALID_CHANNEL = 0x10000000;

    private static final String CHANNEL_STRING_CANARY = "Chrome Canary";
    private static final String CHANNEL_STRING_DEV = "Chrome Dev";
    private static final String CHANNEL_STRING_BETA = "Chrome Beta";
    private static final String CHANNEL_STRING_STABLE = "Chrome";

    private static int sChannel = INVALID_CHANNEL;

    /**
     * This must be called before any other method in this class is called.
     * @param context The context to query the build channel from.
     */
    public static void init(Context context) {
        final String channel = BuildInfo.getPackageLabel(context);
        if (CHANNEL_STRING_STABLE.equals(channel)) {
            sChannel = CHANNEL_STABLE;
        } else if (CHANNEL_STRING_BETA.equals(channel)) {
            sChannel = CHANNEL_BETA;
        } else if (CHANNEL_STRING_DEV.equals(channel)) {
            sChannel = CHANNEL_DEV;
        } else if (CHANNEL_STRING_CANARY.equals(channel)) {
            sChannel = CHANNEL_CANARY;
        } else {
            sChannel = CHANNEL_UNKNOWN;
        }
    }

    /**
     * @return Whether this build is a local build.
     */
    public static boolean isLocalBuild() {
        return getBuildChannel() == CHANNEL_UNKNOWN;
    }

    /**
     * @return Whether this build is a canary build.
     */
    public static boolean isCanaryBuild() {
        return getBuildChannel() == CHANNEL_CANARY;
    }

    /**
     * @return Whether this build is a dev build.
     */
    public static boolean isDevBuild() {
        return getBuildChannel() == CHANNEL_DEV;
    }

    /**
     * @return Whether this build is a beta build.
     */
    public static boolean isBetaBuild() {
        return getBuildChannel() == CHANNEL_BETA;
    }

    /**
     * @return Whether this build is a stable build.
     */
    public static boolean isStableBuild() {
        return getBuildChannel() == CHANNEL_STABLE;
    }

    /**
     * Determines which channel this build is.
     * @return What channel this build is. Can be one of {@link #CHANNEL_UNKNOWN},
     *         {@link #CHANNEL_CANARY}, {@link #CHANNEL_DEV}, {@link #CHANNEL_BETA}, or
     *         {@link #CHANNEL_STABLE}.
     */
    public static int getBuildChannel() {
        if (sChannel == INVALID_CHANNEL) {
            throw new RuntimeException("ChannelInfo.init() was not called");
        }
        return sChannel;
    }

    /**
     * @return Whether this is an official (i.e. Google Chrome) build.
     */
    public static boolean isOfficialBuild() {
        return ChromeVersionConstants.IS_OFFICIAL_BUILD;
    }
}
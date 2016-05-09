// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;

/**
 * Java accessor for base/feature_list.h state.
 */
@JNINamespace("chrome::android")
@MainDex
public abstract class ChromeFeatureList {
    // Prevent instantiation.
    private ChromeFeatureList() {}

    /**
     * Returns whether the specified feature is enabled or not.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/android/chrome_feature_list.cc
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     */
    public static boolean isEnabled(String featureName) {
        return nativeIsEnabled(featureName);
    }

    public static final String MEDIA_STYLE_NOTIFICATION = "MediaStyleNotification";
    public static final String NTP_FAKE_OMNIBOX_TEXT = "NTPFakeOmniboxText";
    public static final String NTP_MATERIAL_DESIGN = "NTPMaterialDesign";
    public static final String NTP_SNIPPETS = "NTPSnippets";
    public static final String NTP_TOOLBAR = "NTPToolbar";

    private static native boolean nativeIsEnabled(String featureName);
}

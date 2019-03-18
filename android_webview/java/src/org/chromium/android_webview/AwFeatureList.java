// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;

/**
 * Java accessor for base/feature_list.h state.
 */
@JNINamespace("android_webview")
@MainDex
final public class AwFeatureList {
    // Do not instantiate this class.
    private AwFeatureList() {}

    private static final String GMS_PACKAGE = "com.google.android.gms";

    private static Boolean sPageStartedOnCommitForBrowserNavigations;

    private static boolean computePageStartedOnCommitForBrowserNavigations() {
        if (!nativeIsEnabled(WEBVIEW_PAGE_STARTED_ON_COMMIT)) return false;
        if (GMS_PACKAGE.equals(ContextUtils.getApplicationContext().getPackageName())) {
            try {
                PackageInfo gmsPackage =
                        ContextUtils.getApplicationContext().getPackageManager().getPackageInfo(
                                GMS_PACKAGE, 0);
                return gmsPackage.versionCode >= 15000000;
            } catch (PackageManager.NameNotFoundException e) {
            }
            return false;
        }
        return true;
    }

    public static boolean pageStartedOnCommitEnabled(boolean isRendererInitiated) {
        // Always enable for renderer-initiated navigations.
        if (isRendererInitiated) return true;
        if (sPageStartedOnCommitForBrowserNavigations != null) {
            return sPageStartedOnCommitForBrowserNavigations;
        }
        sPageStartedOnCommitForBrowserNavigations =
                computePageStartedOnCommitForBrowserNavigations();
        return sPageStartedOnCommitForBrowserNavigations;
    }

    /**
     * Returns whether the specified feature is enabled or not.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in android_webview/browser/aw_feature_list.cc
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     */
    public static boolean isEnabled(String featureName) {
        return nativeIsEnabled(featureName);
    }

    // Alphabetical:
    public static final String WEBVIEW_CONNECTIONLESS_SAFE_BROWSING =
            "WebViewConnectionlessSafeBrowsing";
    public static final String WEBVIEW_PAGE_STARTED_ON_COMMIT = "WebViewPageStartedOnCommit";

    private static native boolean nativeIsEnabled(String featureName);
}

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;

/**
 * The model and controller for a group of explore options.
 */
@JNINamespace("explore_sites")
public class ExploreSitesBridge {
    private static final String TAG = "ExploreSitesBridge";

    /**
     * Fetches the catalog data for Explore page.
     *
     * Callback will be called with |null| if an error occurred.
     */
    public static void getEspCatalog(
            Profile profile, Callback<List<ExploreSitesCategory>> callback) {
        List<ExploreSitesCategory> result = new ArrayList<>();
        nativeGetEspCatalog(profile, result, callback);
    }

    public static void getSiteImage(Profile profile, int siteID, Callback<Bitmap> callback) {
        nativeGetIcon(profile, siteID, callback);
    }

    public static void getCategoryImage(
            Profile profile, int categoryID, int pixelSize, Callback<Bitmap> callback) {
        // TODO(dewittj): Remove this when image decoding works correctly.
        Bitmap image = Bitmap.createBitmap(pixelSize, pixelSize, Bitmap.Config.ARGB_8888);
        image.eraseColor(android.graphics.Color.GREEN);
        callback.onResult(image);
    }

    /**
     * Causes a network request for updating the catalog.
     */
    public static void updateCatalogFromNetwork(
            Profile profile, boolean isImmediateFetch, Callback<Boolean> finishedCallback) {
        nativeUpdateCatalogFromNetwork(profile, isImmediateFetch, finishedCallback);
    }

    /**
     * Gets the current Finch variation that is configured by flag or experiment.
     */
    @ExploreSitesVariation
    public static int getVariation() {
        return nativeGetVariation();
    }

    @CalledByNative
    static void scheduleDailyTask() {
        ExploreSitesBackgroundTask.schedule(false /* updateCurrent */);
    }

    static native int nativeGetVariation();
    private static native void nativeGetEspCatalog(Profile profile,
            List<ExploreSitesCategory> result, Callback<List<ExploreSitesCategory>> callback);

    private static native void nativeGetIcon(
            Profile profile, int siteID, Callback<Bitmap> callback);

    private static native void nativeUpdateCatalogFromNetwork(
            Profile profile, boolean isImmediateFetch, Callback<Boolean> callback);
}

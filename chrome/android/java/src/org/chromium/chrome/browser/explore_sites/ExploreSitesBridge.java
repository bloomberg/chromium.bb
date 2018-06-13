// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
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

    private long mNativeExploreSitesBridge;

    /**
     * Creates an explore sites bridge for a given profile.
     */
    public ExploreSitesBridge(Profile profile) {
        mNativeExploreSitesBridge = nativeInit(profile);
    }

    /**
     * Fetches a JSON string from URL, returning the parsed JSONobject in a callback.
     */
    public void getNtpCategories(
            final String url, final Callback<List<ExploreSitesCategoryTile>> callback) {
        List<ExploreSitesCategoryTile> result = new ArrayList<>();
        nativeGetNtpCategories(mNativeExploreSitesBridge, url, result, callback);
    }

    /**
     * Fetches an icon from a url and returns in a Bitmap image.
     */
    public void getIcon(final String iconUrl, final Callback<Bitmap> callback) {}

    /**
     * Destroys the native object. Call only when no longer needed.
     */
    public void destroy() {
        nativeDestroy(mNativeExploreSitesBridge);
    }

    private native long nativeInit(Profile profile);
    private native void nativeGetNtpCategories(long nativeExploreSitesBridge, String url,
            List<ExploreSitesCategoryTile> result,
            Callback<List<ExploreSitesCategoryTile>> callback);
    private native void nativeDestroy(long nativeExploreSitesBridge);
}

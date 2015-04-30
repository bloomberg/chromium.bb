// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.favicon;

import android.graphics.Bitmap;

import org.chromium.base.CalledByNative;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * A Java API for using the C++ LargeIconService.
 *
 * An instance of this class must be created, used, and destroyed on the same thread.
 */
public class LargeIconBridge {

    private long mNativeLargeIconBridge;

    /**
     * Callback for use with GetLargeIconForUrl().
     */
    public interface LargeIconCallback {
        /**
         * Called when the icon or fallback color is available.
         *
         * @param icon The icon, or null if none is available.
         * @param fallbackColor The fallback color to use if icon is null.
         */
        @CalledByNative("LargeIconCallback")
        void onLargeIconAvailable(Bitmap icon, int fallbackColor);
    }

    /**
     * Initializes the C++ side of this class.
     */
    public LargeIconBridge() {
        mNativeLargeIconBridge = nativeInit();
    }

    /**
     * Deletes the C++ side of this class. This must be called when this object is no longer needed.
     */
    public void destroy() {
        assert mNativeLargeIconBridge != 0;
        nativeDestroy(mNativeLargeIconBridge);
        mNativeLargeIconBridge = 0;
    }

    /**
     * Given a URL, returns a large icon for that URL if one is available (e.g. a favicon or
     * touch icon). If none is available, a fallback color is returned, based on the dominant color
     * of any small icons for the URL, or a default gray if no small icons are available. The icon
     * and fallback color are returned asynchronously to the given callback.
     *
     * @param profile Profile to use when fetching icons.
     * @param pageUrl The URL of the page whose icon will be fetched.
     * @param desiredSizePx The desired size of the icon in pixels.
     * @param callback The method to call asynchronously when the result is available. This callback
     *                 will not be called if this method returns false.
     * @return True if a callback should be expected.
     */
    public boolean getLargeIconForUrl(Profile profile, String pageUrl,
            int desiredSizePx, LargeIconCallback callback) {
        assert mNativeLargeIconBridge != 0;
        return nativeGetLargeIconForURL(mNativeLargeIconBridge, profile, pageUrl,
                desiredSizePx, callback);
    }

    private static native long nativeInit();
    private static native void nativeDestroy(long nativeLargeIconBridge);
    private static native boolean nativeGetLargeIconForURL(long nativeLargeIconBridge,
            Profile profile, String pageUrl, int desiredSizePx, LargeIconCallback callback);
}

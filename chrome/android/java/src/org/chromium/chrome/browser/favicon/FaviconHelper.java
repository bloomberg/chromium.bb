// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.favicon;

import android.graphics.Bitmap;

import org.chromium.base.CalledByNative;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * This is a helper class to use favicon_service.cc's functionality.
 *
 * You can request a favicon image by web page URL. Note that an instance of this class should be
 * created & used & destroyed (by destroy()) in the same thread due to the C++ CancelableTaskTracker
 * class requirement.
 */
public class FaviconHelper {

    // Please keep in sync with favicon_types.h's IconType.
    public static final int INVALID_ICON = 0;
    public static final int FAVICON = 1 << 0;
    public static final int TOUCH_ICON = 1 << 1;
    public static final int TOUCH_PRECOMPOSED_ICON = 1 << 2;

    private int mNativeFaviconHelper;

    /**
     * Callback interface for getting the result from getFaviconImageForURL method.
     */
    public interface FaviconImageCallback {
        /**
         * This method will be called when the result favicon is ready.
         * @param image   Favicon image.
         * @param iconUrl Favicon image's icon url.
         */
        @CalledByNative("FaviconImageCallback")
        public void onFaviconAvailable(Bitmap image, String iconUrl);
    }

    /**
     * Allocate and initialize the C++ side of this class.
     */
    public FaviconHelper() {
        mNativeFaviconHelper = nativeInit();
    }

    /**
     * Clean up the C++ side of this class. After the call, this class instance shouldn't be used.
     */
    public void destroy() {
        nativeDestroy(mNativeFaviconHelper);
        mNativeFaviconHelper = 0;
    }

    @Override
    protected void finalize() {
        // It is not O.K. to call nativeDestroy() here because garbage collection can be
        // performed in another thread, and CancelableTaskTrack should be destroyed in the
        // same thread where it was created. So we just make sure that destroy() is called before
        // garbage collection picks up by the following assertion.
        assert mNativeFaviconHelper == 0;
    }

    /**
     * Get Favicon bitmap for the requested arguments.
     * @param profile               Profile used for the FaviconService construction.
     * @param pageUrl               The target Page URL to get the favicon.
     * @param iconTypes             One of the IconType class values.
     * @param desiredSizeInDip      The size of the favicon in dip we want to get.
     * @param faviconImageCallback  A method to be called back when the result is available.
     *                              Note that this callback is not called if this method returns
     *                              false.
     * @return                      True if we GetFaviconImageForURL is successfully called.
     */
    public boolean getFaviconImageForURL(
            Profile profile, String pageUrl, int iconTypes,
            int desiredSizeInDip, FaviconImageCallback faviconImageCallback) {
        assert mNativeFaviconHelper != 0;
        return nativeGetFaviconImageForURL(mNativeFaviconHelper, profile, pageUrl, iconTypes,
                desiredSizeInDip, faviconImageCallback);
    }

    private static native int nativeInit();
    private static native void nativeDestroy(int nativeFaviconHelper);
    private static native boolean nativeGetFaviconImageForURL(int nativeFaviconHelper,
            Profile profile, String pageUrl, int iconTypes, int desiredSizeInDip,
            FaviconImageCallback faviconImageCallback);
}

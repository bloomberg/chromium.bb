// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.graphics.Bitmap;

/**
 * Interface for building WebAPKs.
 */
public interface WebApkBuilder {
    /**
     * Asynchronously build WebAPK.
     * @param startUrl        The url that the WebAPK should navigate to when launched from the app
     *                        list.
     * @param scope           Navigations from the WebAPK to URLs with sub-origin {@link scope} will
     *                        stay within the WebAPK.
     * @param name            "name" from the Web Manifest.
     * @param shortName       "short_name" from the Web Manifest.
     * @param iconUrl         Url of the icon for the app list. Url of the favicon if the Web
     *                        Manifest does not specify an icon.
     * @param icon            Icon for the app list. Generated icon or favicon if the Web Manifest
     *                        does not specify an icon.
     * @param displayMode     "display" from the Web Manifest.
     * @param orientation     "orientation" from the Web Manifest.
     * @param themeColor      "theme_color" from the Web Manifest.
     * @param backgroundColor "background_color" from the Web Manifest.
     * @param manifestUrl     The URL of the Web Manifest.
     */
    void buildWebApkAsync(String startUrl, String scope, String name, String shortName,
            String iconUrl, Bitmap icon, int displayMode, int orientation, long themeColor,
            long backgroundColor, String manifestUrl);
}

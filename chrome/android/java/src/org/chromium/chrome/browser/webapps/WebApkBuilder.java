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
     * @param startUrl The url that the WebAPK should navigate to when launched from the app list.
     * @param scope Navigations from the WebAPK to URLs with sub-origin {@link scope} will stay
     *               within the WebAPK.
     * @param appName Name for the app list.
     * @param icon Icon for the app list.
     */
    void buildWebApkAsync(String startUrl, String scope, String appName, Bitmap appIcon);
}

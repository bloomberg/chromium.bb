// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;

import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * A class containing some utility static methods.
 */
public class DownloadUtils {

    /**
     * Displays the download manager UI. Note the UI is different on tablets and on phones.
     */
    public static void showDownloadManager(Activity activity, Tab tab) {
        if (DeviceFormFactor.isTablet(activity)) {
            tab.loadUrl(new LoadUrlParams(UrlConstants.DOWNLOADS_URL));
        } else {
            DownloadActivity.launch(activity);
        }
    }
}

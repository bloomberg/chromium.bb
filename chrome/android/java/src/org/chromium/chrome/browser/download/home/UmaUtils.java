// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import android.support.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Utility methods related to metrics collection on download home. */
public class UmaUtils {
    // Please treat this list as append only and keep it in sync with
    // Android.DownloadManager.Menu.Actions in enums.xml.
    @IntDef({MenuAction.CLOSE, MenuAction.MULTI_DELETE, MenuAction.MULTI_SHARE,
            MenuAction.SHOW_INFO, MenuAction.HIDE_INFO, MenuAction.SEARCH})
    @Retention(RetentionPolicy.SOURCE)
    public @interface MenuAction {
        int CLOSE = 0;
        int MULTI_DELETE = 1;
        int MULTI_SHARE = 2;
        int SHOW_INFO = 3;
        int HIDE_INFO = 4;
        int SEARCH = 5;
        int NUM_ENTRIES = 6;
    }

    /**
     * Called to record metrics for the given menu action.
     * @param action The given menu action.
     */
    public static void recordMenuActionHistogram(@MenuAction int action) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.DownloadManager.Menu.Action", action, MenuAction.NUM_ENTRIES);
    }
}

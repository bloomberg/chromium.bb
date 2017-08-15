// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.chromium.chrome.browser.download.DownloadNotificationService.ACTION_DOWNLOAD_UPDATE_SUMMARY_ICON;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/**
 * This {@link BroadcastReceiver} handles clicks to download notifications and their action buttons.
 * Clicking on an in-progress or failed download will open the download manager. Clicking on
 * a complete, successful download will open the file. Clicking on the resume button of a paused
 * download will relaunch the browser process and try to resume the download from where it is
 * stopped.
 */
public class DownloadBroadcastReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(final Context context, Intent intent) {
        if (ACTION_DOWNLOAD_UPDATE_SUMMARY_ICON.equals(intent.getAction())) {
            DownloadNotificationService.startDownloadNotificationService(context, intent);
        }
    }
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Notification;
import android.content.Context;

import java.util.ArrayList;
import java.util.List;

/**
 * Mock class to DownloadNotificationService for testing purpose.
 */
public class MockDownloadNotificationService extends DownloadNotificationService {
    private final List<Integer> mNotificationIds = new ArrayList<Integer>();
    private boolean mPaused = false;
    private Context mContext;

    void setContext(Context context) {
        mContext = context;
    }

    @Override
    public void pauseAllDownloads() {
        super.pauseAllDownloads();
        mPaused = true;
    }

    @Override
    void updateNotification(int id, Notification notification) {
        mNotificationIds.add(id);
    }

    public boolean isPaused() {
        return mPaused;
    }

    public List<Integer> getNotificationIds() {
        return mNotificationIds;
    }

    @Override
    public void cancelNotification(int downloadId) {
        mNotificationIds.remove(downloadId);
    }

    @Override
    public Context getApplicationContext() {
        return mContext == null ? super.getApplicationContext() : mContext;
    }

}


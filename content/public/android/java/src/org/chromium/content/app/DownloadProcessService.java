// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.app;

import android.annotation.SuppressLint;
import android.app.Notification;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;

import org.chromium.base.Log;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content.R;
import org.chromium.content.browser.ChildProcessConstants;
import org.chromium.content.common.IChildProcessCallback;

/**
 * Background download process for handling all transferable downloads.
 */
@JNINamespace("content")
public class DownloadProcessService extends ChildProcessService {
    private static final String TAG = "DownloadProcess";
    private long mClientContext;
    private IChildProcessCallback mCallback;
    private int mDownloadCount;

    @Override
    public void onCreate() {
        super.onCreate();
        // TODO(qinmin): Use the first pending download as notification, or
        // get a more proper notification for this.
        startForeground(R.id.download_service_notification, new Notification());
    }

    @Override
    @SuppressLint("NewApi")
    public int onStartCommand(Intent intent, int flags, int startId) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2;
        initializeParams(intent);
        Bundle bundle = intent.getExtras();
        if (bundle != null) {
            IBinder binder = bundle.getBinder(ChildProcessConstants.EXTRA_CHILD_PROCESS_CALLBACK);
            mCallback = IChildProcessCallback.Stub.asInterface(binder);
            getServiceInfo(bundle);
        }
        return START_STICKY;
    }

    /**
     * Will be called by the native side when a download starts or is rejected.
     *
     * @CalledByNative
     */
    private void onDownloadStarted(boolean started, int downloadId) {
        if (mCallback != null) {
            try {
                mCallback.onDownloadStarted(started, downloadId);
            } catch (RemoteException e) {
                Log.e(TAG, "Unable to callback the browser process.", e);
            }
        }
        if (started) mDownloadCount++;
    }

    /**
     * Will be called by the native side when a download completes.
     *
     * @CalledByNative
     */
    private void onDownloadCompleted(boolean success) {
        mDownloadCount--;
        if (mDownloadCount == 0) stopSelf();
    }
}
// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.Log;
import org.chromium.chrome.browser.AppHooks;

/**
 * MediaButtonReceiver is a basic BroadcastReceiver class that receives
 * ACTION_MEDIA_BUTTON from a MediaSessionCompat. It then forward these intents
 * to the service listening to them.
 * This is there for backward compatibility with JB_MR0 and JB_MR1.
 */
public abstract class MediaButtonReceiver extends BroadcastReceiver {
    public abstract Class<?> getServiceClass();

    private static final String TAG = "MediaButtonReceiver";

    @Override
    public void onReceive(Context context, Intent intent) {
        intent.setClass(context, getServiceClass());
        Log.i(TAG, "Receive broadcast message, starting foreground service");
        AppHooks.get().startForegroundService(intent);
    }
}

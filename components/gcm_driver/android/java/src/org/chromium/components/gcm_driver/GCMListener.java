// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver;

import android.content.Intent;

import com.google.ipc.invalidation.external.client.contrib.MultiplexingGcmListener;

import org.chromium.base.ThreadUtils;

/**
 * Receives GCM registration events and messages rebroadcast by MultiplexingGcmListener.
 */
public class GCMListener extends MultiplexingGcmListener.AbstractListener {
    /**
     * Receiver for broadcasts by the multiplexed GCM service. It forwards them to
     * GCMListener.
     *
     * This class is public so that it can be instantiated by the Android runtime.
     */
    public static class Receiver extends MultiplexingGcmListener.AbstractListener.Receiver {
        @Override
        protected Class<?> getServiceClass() {
            return GCMListener.class;
        }
    }

    private static final String TAG = "GCMListener";

    // Used as a fallback since GCM doesn't yet give us the app ID.
    // TODO(johnme): Get real app IDs from GCM, and remove this.
    private static final String UNKNOWN_APP_ID = "push#https://www.gcmlistenerfake.com#0";

    public GCMListener() {
        super(TAG);
    }

    @Override
    protected void onRegistered(final String registrationId) {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override public void run() {
                GCMDriver.onRegisterFinished(UNKNOWN_APP_ID, registrationId);
            }
        });
    }

    @Override
    protected void onUnregistered(String registrationId) {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override public void run() {
                GCMDriver.onUnregisterFinished(UNKNOWN_APP_ID);
            }
        });
    }

    @Override
    protected void onMessage(final Intent intent) {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override public void run() {
                GCMDriver.onMessageReceived(getApplicationContext(), UNKNOWN_APP_ID,
                    intent.getExtras());
            }
        });
    }

    @Override
    protected void onDeletedMessages(int total) {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override public void run() {
                GCMDriver.onMessagesDeleted(getApplicationContext(), UNKNOWN_APP_ID);
            }
        });
    }
}

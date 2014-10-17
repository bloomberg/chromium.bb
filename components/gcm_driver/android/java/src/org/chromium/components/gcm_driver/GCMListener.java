// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver;

import android.content.Intent;
import android.os.Bundle;

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

    // Used as a fallback until GCM always gives us the subtype.
    // TODO(johnme): Remove this once it does.
    static final String UNKNOWN_APP_ID = "push#https://www.gcmlistenerfake.com#0";

    public GCMListener() {
        super(TAG);
    }

    @Override
    protected void onRegistered(String registrationId) {
        // Ignore this, since we register using GoogleCloudMessagingV2.
    }

    @Override
    protected void onUnregistered(String registrationId) {
        // Ignore this, since we register using GoogleCloudMessagingV2.
    }

    @Override
    protected void onMessage(final Intent intent) {
        final String bundleSubtype = "subtype";
        final String bundleDataForPushApi = "data";
        Bundle extras = intent.getExtras();
        if (!extras.containsKey(bundleSubtype) && !extras.containsKey(bundleDataForPushApi)) {
            // TODO(johnme): Once GCM always gives us the subtype, we'll be able to early-out if
            // there is no subtype extra. For now we have to also allow messages without subtype
            // if they have the data key which suggests they are for the Push API, but this is
            // technically a layering violation as this code is for other consumers of GCM too.
            return;
        }
        final String appId = extras.containsKey(bundleSubtype) ? extras.getString(bundleSubtype)
                                                               : UNKNOWN_APP_ID;
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override public void run() {
                GCMDriver.onMessageReceived(getApplicationContext(), appId,
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

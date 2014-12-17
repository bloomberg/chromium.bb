// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.apiary;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Looper;

import com.google.ipc.invalidation.external.client.contrib.MultiplexingGcmListener;

import java.io.IOException;
import java.util.concurrent.CountDownLatch;

/**
 * Helps using MultiplexingGcmListene in blocking manner. If the app has not registered in GCM
 * it sends registration request and waits for an intent with registration ID.
 * Waiting may be interrupted. Must not be used on UI (or Context's main) looper.
 */
public final class BlockingGCMRegistrar {
    public String blockingGetRegistrationId(Context context)
            throws InterruptedException, IOException {
        assert context != null;
        assert context.getMainLooper() != Looper.myLooper();

        Receiver receiver = new Receiver();
        receiver.register(context);
        try {
            // MultiplexingGcmListener starts registration if the app has not registered yet.
            String result = MultiplexingGcmListener.initializeGcm(context);
            if (result != null && !result.isEmpty()) return result;
            return receiver.awaitRegistrationId();
        } finally {
            receiver.unregister(context);
        }
    }

    private static class Receiver extends BroadcastReceiver {
        private final CountDownLatch mDone = new CountDownLatch(1);

        private String mRegistrationId;

        public void register(Context context) {
            IntentFilter filter = new IntentFilter();
            filter.addCategory(context.getPackageName());
            filter.addAction(MultiplexingGcmListener.Intents.ACTION);
            context.registerReceiver(this, filter);
        }

        public void unregister(Context context) {
            context.unregisterReceiver(this);
        }

        public String awaitRegistrationId() throws InterruptedException, IOException {
            mDone.await();
            if (mRegistrationId != null) {
                return mRegistrationId;
            }
            throw new IOException();
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            assert intent.getAction().equals(MultiplexingGcmListener.Intents.ACTION);

            if (!intent.getBooleanExtra(
                    MultiplexingGcmListener.Intents.EXTRA_OP_REGISTERED, false)) {
                return;
            }

            mRegistrationId = intent.getStringExtra(
                    MultiplexingGcmListener.Intents.EXTRA_DATA_REG_ID);

            if (mRegistrationId != null) {
                mDone.countDown();
            }
        }
    }
}

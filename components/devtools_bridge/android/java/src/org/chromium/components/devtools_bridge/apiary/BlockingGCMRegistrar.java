// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.apiary;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.util.Log;

import com.google.android.gcm.GCMConstants;
import com.google.android.gcm.GCMRegistrar;

import java.io.IOException;
import java.util.concurrent.CountDownLatch;

/**
 * Helps using GCDRegistrar in blocking manner. If the app has not registered in GCM
 * it sends registration request and waits for an intent with registration ID.
 * Waiting may be interrupted. Must not be used on UI (or Context's main) looper.
 */
public abstract class BlockingGCMRegistrar {
    private static final String TAG = "BlockingGCMRegistrar";

    public String blockingGetRegistrationId(Context context)
            throws InterruptedException, IOException {
        assert context != null;

        Receiver receiver = new Receiver();
        receiver.register(context);
        try {
            String result = GCMRegistrar.getRegistrationId(context);
            if (result != null && !result.isEmpty()) return result;
            GCMRegistrar.register(context, getSenderIds());
            return receiver.awaitRegistrationId();
        } finally {
            receiver.unregister(context);
        }
    }

    protected abstract String[] getSenderIds();

    private static class Receiver extends BroadcastReceiver {
        private final CountDownLatch mDone = new CountDownLatch(1);

        private String mRegistrationId;
        private String mError;

        public void register(Context context) {
            IntentFilter filter = new IntentFilter();
            filter.addCategory(context.getPackageName());
            filter.addAction(GCMConstants.INTENT_FROM_GCM_REGISTRATION_CALLBACK);
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
            throw new IOException(mError);
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            assert intent.getAction().equals(GCMConstants.INTENT_FROM_GCM_REGISTRATION_CALLBACK);

            mRegistrationId = intent.getStringExtra(GCMConstants.EXTRA_REGISTRATION_ID);
            mError = intent.getStringExtra(GCMConstants.EXTRA_ERROR);

            if (mRegistrationId != null || mError != null) {
                mDone.countDown();
            } else {
                Log.e(TAG, "Unexpected intent: " + intent);
            }
        }
    }
}

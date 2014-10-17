// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.Messenger;

import java.io.IOException;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;


/**
 * Temporary code for sending subtypes when (un)registering with GCM.
 * Subtypes are experimental and may change without notice!
 * TODO(johnme): Remove this file, once we switch to the GMS client library.
 */
public class GoogleCloudMessagingV2 {

    // Inlined com.google.android.gms.common.GooglePlayServicesUtil.GOOGLE_PLAY_SERVICES_PACKAGE
    // since this class mustn't depend on the GMS client library.
    private static final String GOOGLE_PLAY_SERVICES_PACKAGE = "com.google.android.gms";
    private static final long REGISTER_TIMEOUT = 5000;
    private static final String ACTION_C2DM_REGISTER =
            "com.google.android.c2dm.intent.REGISTER";
    private static final String ACTION_C2DM_UNREGISTER =
            "com.google.android.c2dm.intent.UNREGISTER";
    private static final String C2DM_EXTRA_ERROR = "error";
    private static final String INTENT_PARAM_APP = "app";
    private static final String ERROR_MAIN_THREAD = "MAIN_THREAD";
    private static final String ERROR_SERVICE_NOT_AVAILABLE = "SERVICE_NOT_AVAILABLE";
    private static final String EXTRA_REGISTRATION_ID = "registration_id";
    private static final String EXTRA_UNREGISTERED = "unregistered";
    private static final String EXTRA_SENDER = "sender";
    private static final String EXTRA_MESSENGER = "google.messenger";
    private static final String EXTRA_SUBTYPE = "subtype";

    private Context mContext;
    private PendingIntent mAppPendingIntent = null;
    private Object mAppPendingIntentLock = new Object();

    public GoogleCloudMessagingV2(Context context) {
        mContext = context;
    }

    /**
     * Register the application for GCM and return the registration ID. You must call this once,
     * when your application is installed, and send the returned registration ID to the server.
     * <p>
     * This is a blocking call&mdash;you shouldn't call it from the UI thread.
     * <p>
     * Repeated calls to this method will return the original registration ID.
     * <p>
     * If you want to modify the list of senders, you must call {@code unregister()} first.
     * <p>
     * Most applications use a single sender ID. You may use multiple senders if different
     * servers may send messages to the app or for testing.
     *
     * @param senderIds list of project numbers or Google accounts identifying who is allowed to
     *   send messages to this application.
     * @return registration id
     */
    public String register(String subtype, String... senderIds) throws IOException {
        if (Looper.getMainLooper() == Looper.myLooper()) {
            throw new IOException(ERROR_MAIN_THREAD);
        }

        final BlockingQueue<Intent> registerResult = new LinkedBlockingQueue<Intent>();
        Handler registrationHandler = new Handler(Looper.getMainLooper()) {
            @Override
            public void handleMessage(Message msg) {
                Intent res = (Intent) msg.obj;
                registerResult.add(res);
            }
        };
        Messenger messenger = new Messenger(registrationHandler);

        internalRegister(messenger, subtype, senderIds);

        try {
            Intent regIntent = registerResult.poll(REGISTER_TIMEOUT, TimeUnit.MILLISECONDS);
            if (regIntent == null) {
                throw new IOException(ERROR_SERVICE_NOT_AVAILABLE);
            }
            String registrationId = regIntent.getStringExtra(EXTRA_REGISTRATION_ID);
            // registration succeeded
            if (registrationId != null) {
                return registrationId;
            }
            String err = regIntent.getStringExtra(C2DM_EXTRA_ERROR);
            if (err != null) {
                throw new IOException(err);
            } else {
                throw new IOException(ERROR_SERVICE_NOT_AVAILABLE);
            }
        } catch (InterruptedException e) {
            throw new IOException(e.getMessage());
        }
    }

    private void internalRegister(Messenger messenger, String subtype, String... senderIds) {
        Intent intent = new Intent(ACTION_C2DM_REGISTER);
        intent.setPackage(GOOGLE_PLAY_SERVICES_PACKAGE);
        if (subtype != null) intent.putExtra("subtype", subtype);
        intent.putExtra(EXTRA_MESSENGER, messenger);
        setPackageNameExtra(intent);
        intent.putExtra(EXTRA_SENDER, getFlatSenderIds(senderIds));
        mContext.startService(intent);
    }

    private String getFlatSenderIds(String... senderIds) {
        if (senderIds == null || senderIds.length == 0) {
            throw new IllegalArgumentException("No senderIds");
        }
        StringBuilder builder = new StringBuilder(senderIds[0]);
        for (int i = 1; i < senderIds.length; i++) {
            builder.append(',').append(senderIds[i]);
        }
        return builder.toString();
    }

    private void setPackageNameExtra(Intent intent) {
        synchronized (mAppPendingIntentLock) {
            if (mAppPendingIntent == null) {
                Intent target = new Intent();
                // Fill in the package, to prevent the intent from being used.
                target.setPackage("com.google.example.invalidpackage");
                mAppPendingIntent = PendingIntent.getBroadcast(
                    mContext.getApplicationContext(), 0, target, 0);
            }
        }
        intent.putExtra(INTENT_PARAM_APP, mAppPendingIntent);
    }

    /**
     * Unregister the application. Calling {@code unregister()} stops any
     * messages from the server.
     * <p>
     * This is a blocking call&mdash;you shouldn't call it from the UI thread.
     * <p>
     * You should rarely (if ever) need to call this method. Not only is it
     * expensive in terms of resources, but it invalidates your registration ID,
     * which you should never change unnecessarily. A better approach is to simply
     * have your server stop sending messages. Only use unregister if you want
     * to change your sender ID.
     *
     * @throws IOException if we can't connect to server to unregister.
     */
    public void unregister(String subtype) throws IOException {
        if (Looper.getMainLooper() == Looper.myLooper()) {
            throw new IOException(ERROR_MAIN_THREAD);
        }
        final BlockingQueue<Intent> registerResult = new LinkedBlockingQueue<Intent>();
        Handler registrationHandler = new Handler(Looper.getMainLooper()) {
            @Override
            public void handleMessage(Message msg) {
                Intent res = (Intent) msg.obj;
                registerResult.add(res);
            }
        };
        Messenger messenger = new Messenger(registrationHandler);
        internalUnregister(messenger, subtype);
        try {
            Intent regIntent = registerResult.poll(REGISTER_TIMEOUT, TimeUnit.MILLISECONDS);
            if (regIntent == null) {
                throw new IOException(ERROR_SERVICE_NOT_AVAILABLE);
            }
            String unregistered = regIntent.getStringExtra(EXTRA_UNREGISTERED);
            if (unregistered != null) {
                // All done
                return;
            }
            String err = regIntent.getStringExtra(C2DM_EXTRA_ERROR);
            if (err != null) {
                throw new IOException(err);
            } else {
                throw new IOException(ERROR_SERVICE_NOT_AVAILABLE);
            }
        } catch (InterruptedException e) {
            throw new IOException(e.getMessage());
        }
    }

    private void internalUnregister(Messenger messenger, String subtype) {
        Intent intent = new Intent(ACTION_C2DM_UNREGISTER);
        intent.setPackage(GOOGLE_PLAY_SERVICES_PACKAGE);
        if (subtype != null) {
            intent.putExtra("subtype", subtype);
        }
        intent.putExtra(EXTRA_MESSENGER, messenger);
        setPackageNameExtra(intent);
        mContext.startService(intent);
    }
}

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver;

import android.content.Context;
import android.os.Bundle;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.ThreadUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * An implementation of GCMDriver using Android's Java GCM APIs.
 * Threading model: all calls to/from C++ happen on the UI thread.
 */
@JNINamespace("gcm")
public final class GCMDriver {
    // The instance of GCMDriver currently owned by a C++ GCMDriverAndroid, if any.
    private static GCMDriver sInstance = null;

    private long mNativeGCMDriverAndroid;
    private final Context mContext;

    private GCMDriver(long nativeGCMDriverAndroid, Context context) {
        mNativeGCMDriverAndroid = nativeGCMDriverAndroid;
        mContext = context;
    }

    /**
     * Create a GCMDriver object, which is owned by GCMDriverAndroid
     * on the C++ side.
     *
     * @param nativeGCMDriverAndroid The C++ object that owns us.
     * @param context The app context.
     */
    @CalledByNative
    private static GCMDriver create(long nativeGCMDriverAndroid,
                                    Context context) {
        if (sInstance != null)
            throw new IllegalStateException("Already instantiated");
        sInstance = new GCMDriver(nativeGCMDriverAndroid, context);
        return sInstance;
    }

    /**
     * Called when our C++ counterpart is deleted. Clear the handle to our
     * native C++ object, ensuring it's never called.
     */
    @CalledByNative
    private void destroy() {
        assert sInstance == this;
        sInstance = null;
        mNativeGCMDriverAndroid = 0;
    }

    @CalledByNative
    private void register(String appId, String[] senderIds) {
        // TODO(johnme): Actually try to register.
        nativeOnRegisterFinished(mNativeGCMDriverAndroid, appId, "", false);
    }
    @CalledByNative
    private void unregister(String appId) {
        // TODO(johnme): Actually try to unregister.
        nativeOnUnregisterFinished(mNativeGCMDriverAndroid, appId, false);
    }

    public static void onRegistered(String appId, String registrationId) {
        ThreadUtils.assertOnUiThread();
        // TODO(johnme): Update registrations cache?
        if (sInstance != null)
            sInstance.nativeOnRegisterFinished(sInstance.mNativeGCMDriverAndroid, appId,
                                               registrationId, true);
    }

    public static void onUnregistered(String appId) {
        ThreadUtils.assertOnUiThread();
        // TODO(johnme): Update registrations cache?
        if (sInstance != null)
            sInstance.nativeOnUnregisterFinished(sInstance.mNativeGCMDriverAndroid, appId, true);
    }

    public static void onMessageReceived(final String appId, final Bundle extras) {
        // TODO(johnme): Store message and redeliver later if Chrome is killed before delivery.
        ThreadUtils.assertOnUiThread();
        launchNativeThen(new Runnable() {
            @Override public void run() {
                final String BUNDLE_SENDER_ID = "from";
                final String BUNDLE_COLLAPSE_KEY = "collapse_key";

                String senderId = extras.getString(BUNDLE_SENDER_ID);
                String collapseKey = extras.getString(BUNDLE_COLLAPSE_KEY);

                List<String> dataKeysAndValues = new ArrayList<String>();
                for (String key : extras.keySet()) {
                    // TODO(johnme): Check there aren't other default keys that we need to exclude.
                    if (key == BUNDLE_SENDER_ID || key == BUNDLE_COLLAPSE_KEY)
                        continue;
                    dataKeysAndValues.add(key);
                    dataKeysAndValues.add(extras.getString(key));
                }

                sInstance.nativeOnMessageReceived(sInstance.mNativeGCMDriverAndroid,
                        appId, senderId, collapseKey,
                        dataKeysAndValues.toArray(new String[dataKeysAndValues.size()]));
            }
        });
    }

    public static void onMessagesDeleted(final String appId) {
        // TODO(johnme): Store event and redeliver later if Chrome is killed before delivery.
        ThreadUtils.assertOnUiThread();
        launchNativeThen(new Runnable() {
            @Override public void run() {
                sInstance.nativeOnMessagesDeleted(sInstance.mNativeGCMDriverAndroid, appId);
            }
        });
    }

    private native void nativeOnRegisterFinished(long nativeGCMDriverAndroid, String appId,
            String registrationId, boolean success);
    private native void nativeOnUnregisterFinished(long nativeGCMDriverAndroid, String appId,
            boolean success);
    private native void nativeOnMessageReceived(long nativeGCMDriverAndroid, String appId,
            String senderId, String collapseKey, String[] dataKeysAndValues);
    private native void nativeOnMessagesDeleted(long nativeGCMDriverAndroid, String appId);

    private static void launchNativeThen(Runnable task) {
        if (sInstance != null) {
            task.run();
            return;
        }

        throw new UnsupportedOperationException("Native startup not yet implemented");
    }
}

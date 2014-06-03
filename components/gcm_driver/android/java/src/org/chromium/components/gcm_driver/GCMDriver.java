// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver;

import android.content.Context;
import android.os.Bundle;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

import java.util.ArrayList;
import java.util.List;

/**
 * An implementation of GCMDriver using Android's Java GCM APIs.
 * Threading model: all calls to/from C++ happen on the UI thread.
 */
@JNINamespace("gcm")
public final class GCMDriver {
    private long mNativeGCMDriverAndroid;
    private Context mContext;

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
        return new GCMDriver(nativeGCMDriverAndroid, context);
    }

    /**
     * Called when our C++ counterpart is deleted. Clear the handle to our
     * native C++ object, ensuring it's never called.
     */
    @CalledByNative
    private void destroy() {
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

    public void onRegistered(String appId, String registrationId) {
        nativeOnRegisterFinished(mNativeGCMDriverAndroid, appId, registrationId, true);
    }

    public void onUnregistered(String appId) {
        nativeOnUnregisterFinished(mNativeGCMDriverAndroid, appId, true);
    }

    public void onMessageReceived(String appId, Bundle extras) {
        String senderId = extras.getString("from");
        String collapseKey = extras.getString("collapse_key");

        List<String> dataKeysAndValues = new ArrayList<String>();
        for (String key : extras.keySet()) {
            // TODO(johnme): Check there aren't other default keys that we need to exclude.
            if (key == "from" || key == "collapse_key")
                continue;
            dataKeysAndValues.add(key);
            dataKeysAndValues.add(extras.getString(key));
        }

        nativeOnMessageReceived(mNativeGCMDriverAndroid, appId, senderId, collapseKey,
                                dataKeysAndValues.toArray(new String[dataKeysAndValues.size()]));
    }

    public void onMessagesDeleted(String appId) {
        nativeOnMessagesDeleted(mNativeGCMDriverAndroid, appId);
    }

    private native void nativeOnRegisterFinished(long nativeGCMDriverAndroid, String appId,
            String registrationId, boolean success);
    private native void nativeOnUnregisterFinished(long nativeGCMDriverAndroid, String appId,
            boolean success);
    private native void nativeOnMessageReceived(long nativeGCMDriverAndroid, String appId,
            String senderId, String collapseKey, String[] dataKeysAndValues);
    private native void nativeOnMessagesDeleted(long nativeGCMDriverAndroid, String appId);
}
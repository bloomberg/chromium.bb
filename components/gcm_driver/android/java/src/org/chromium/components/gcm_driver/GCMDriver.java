// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver;

import android.content.Context;
import android.os.AsyncTask;
import android.os.Bundle;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * This class is the Java counterpart to the C++ GCMDriverAndroid class.
 * It uses Android's Java GCM APIs to implement GCM registration etc, and
 * sends back GCM messages over JNI.
 *
 * Threading model: all calls to/from C++ happen on the UI thread.
 */
@JNINamespace("gcm")
public class GCMDriver {
    private static final String TAG = "GCMDriver";

    // The instance of GCMDriver currently owned by a C++ GCMDriverAndroid, if any.
    private static GCMDriver sInstance = null;

    private long mNativeGCMDriverAndroid;
    private final Context mContext;
    private GoogleCloudMessagingSubscriber mSubscriber;

    private GCMDriver(long nativeGCMDriverAndroid, Context context) {
        mNativeGCMDriverAndroid = nativeGCMDriverAndroid;
        mContext = context;
        mSubscriber = new GoogleCloudMessagingV2(context);
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
        if (sInstance != null) {
            throw new IllegalStateException("Already instantiated");
        }
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
    private void register(final String appId, final String senderId) {
        new AsyncTask<Void, Void, String>() {
            @Override
            protected String doInBackground(Void... voids) {
                try {
                    String subtype = appId;
                    String registrationId = mSubscriber.subscribe(senderId, subtype, null);
                    return registrationId;
                } catch (IOException ex) {
                    Log.w(TAG, "GCM subscription failed for " + appId + ", " + senderId, ex);
                    return "";
                }
            }
            @Override
            protected void onPostExecute(String registrationId) {
                nativeOnRegisterFinished(mNativeGCMDriverAndroid, appId, registrationId,
                                         !registrationId.isEmpty());
            }
        }.execute();
    }

    @CalledByNative
    private void unregister(final String appId, final String senderId) {
        new AsyncTask<Void, Void, Boolean>() {
            @Override
            protected Boolean doInBackground(Void... voids) {
                try {
                    String subtype = appId;
                    mSubscriber.unsubscribe(senderId, subtype, null);
                    return true;
                } catch (IOException ex) {
                    Log.w(TAG, "GCM unsubscription failed for " + appId + ", " + senderId, ex);
                    return false;
                }
            }

            @Override
            protected void onPostExecute(Boolean success) {
                nativeOnUnregisterFinished(mNativeGCMDriverAndroid, appId, success);
            }
        }.execute();
    }

    // The caller of this function is responsible for ensuring the browser process is initialized.
    public static void onMessageReceived(String appId, String senderId, Bundle extras) {
        // TODO(johnme): Store message and redeliver later if Chrome is killed before delivery.
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            // Change of behaviour, throw exception instead of failing silently with Log.e.
            throw new RuntimeException("Failed to instantiate GCMDriver.");
        }
        final String bundleSubtype = "subtype";
        final String bundleSenderId = "from";
        final String bundleCollapseKey = "collapse_key";
        final String bundleRawData = "rawData";
        final String bundleGcmplex = "com.google.ipc.invalidation.gcmmplex.";

        String collapseKey = extras.getString(bundleCollapseKey);  // May be null.
        byte[] rawData = extras.getByteArray(bundleRawData);  // May be null.

        List<String> dataKeysAndValues = new ArrayList<String>();
        for (String key : extras.keySet()) {
            // TODO(johnme): Check there aren't other keys that we need to exclude.
            if (key.equals(bundleSubtype) || key.equals(bundleSenderId)
                    || key.equals(bundleCollapseKey) || key.equals(bundleRawData)
                    || key.startsWith(bundleGcmplex))
                continue;
            dataKeysAndValues.add(key);
            dataKeysAndValues.add(extras.getString(key));
        }

        sInstance.nativeOnMessageReceived(sInstance.mNativeGCMDriverAndroid,
                appId, senderId, collapseKey, rawData,
                dataKeysAndValues.toArray(new String[dataKeysAndValues.size()]));
    }

    @VisibleForTesting
    public static void overrideSubscriberForTesting(GoogleCloudMessagingSubscriber subscriber) {
        assert sInstance != null;
        assert subscriber != null;
        sInstance.mSubscriber = subscriber;
    }

    private native void nativeOnRegisterFinished(long nativeGCMDriverAndroid, String appId,
            String registrationId, boolean success);
    private native void nativeOnUnregisterFinished(long nativeGCMDriverAndroid, String appId,
            boolean success);
    private native void nativeOnMessageReceived(long nativeGCMDriverAndroid, String appId,
            String senderId, String collapseKey, byte[] rawData, String[] dataKeysAndValues);
}

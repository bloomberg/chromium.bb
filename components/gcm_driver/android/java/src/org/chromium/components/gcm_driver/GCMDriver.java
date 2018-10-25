// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver;

import android.content.Context;
import android.content.SharedPreferences;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.task.AsyncTask;
import org.chromium.components.gcm_driver.instance_id.InstanceIDBridge;

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
    // The max number of most recent messages queued per lazy subscription until
    // Chrome is foregrounded.
    @VisibleForTesting
    public static final int MESSAGES_QUEUE_SIZE = 3;

    // The instance of GCMDriver currently owned by a C++ GCMDriverAndroid, if any.
    private static GCMDriver sInstance;

    private long mNativeGCMDriverAndroid;
    private GoogleCloudMessagingSubscriber mSubscriber;

    private GCMDriver(long nativeGCMDriverAndroid) {
        mNativeGCMDriverAndroid = nativeGCMDriverAndroid;
        mSubscriber = new GoogleCloudMessagingV2();
    }

    /**
     * Create a GCMDriver object, which is owned by GCMDriverAndroid
     * on the C++ side.
     *  @param nativeGCMDriverAndroid The C++ object that owns us.
     *
     */
    @CalledByNative
    private static GCMDriver create(long nativeGCMDriverAndroid) {
        if (sInstance != null) {
            throw new IllegalStateException("Already instantiated");
        }
        sInstance = new GCMDriver(nativeGCMDriverAndroid);
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
        new AsyncTask<String>() {
            @Override
            protected String doInBackground() {
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
        }
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @CalledByNative
    private void unregister(final String appId, final String senderId) {
        new AsyncTask<Boolean>() {
            @Override
            protected Boolean doInBackground() {
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
        }
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    // The caller of this function is responsible for ensuring the browser process is initialized.
    public static void dispatchMessage(GCMMessage message) {
        ThreadUtils.assertOnUiThread();

        if (sInstance == null) {
            throw new RuntimeException("Failed to instantiate GCMDriver.");
        }

        sInstance.nativeOnMessageReceived(sInstance.mNativeGCMDriverAndroid, message.getAppId(),
                message.getSenderId(), message.getCollapseKey(), message.getRawData(),
                message.getDataKeysAndValuesArray());
    }

    /**
     * Stores |message| on disk. Stored Messages for a subscription id will be
     * returned by readMessages(). Only the most recent |MESSAGES_QUEUE_SIZE|
     * messages with distinct collapse keys are kept.
     * @param subscriptionId id of the subscription.
     * @param message The message to be persisted.
     */
    public static void persistMessage(String subscriptionId, GCMMessage message) {
        // Messages are stored as a JSONArray in SharedPreferences. The key is
        // |subscriptionId|. The value is a string representing a JSONArray that
        // contains messages serialized as a JSONObject.

        // Load the persisted messages for this subscription.
        Context context = ContextUtils.getApplicationContext();
        SharedPreferences sharedPrefs =
                context.getSharedPreferences(InstanceIDBridge.PREF_PACKAGE, Context.MODE_PRIVATE);
        // Default is an empty queue if no messages are queued for this subscription.
        String queueString = sharedPrefs.getString(subscriptionId, "[]");
        try {
            JSONArray queueJSON = new JSONArray(queueString);
            if (message.getCollapseKey() != null) {
                queueJSON = filterMessageBasedOnCollapseKey(queueJSON, message.getCollapseKey());
            }
            // TODO(https://crbug.com/882887):Add UMA to record how many
            // messages are currently in the queue, to identify how often we hit
            // the limit.

            // If the queue is full remove the oldest message.
            if (queueJSON.length() == MESSAGES_QUEUE_SIZE) {
                Log.w(TAG,
                        "Dropping GCM Message due queue size limit. Sender id:"
                                + GCMMessage.peekSenderId(queueJSON.getJSONObject(0)));
                JSONArray newQueue = new JSONArray();
                // Copy all messages except the first one.
                for (int i = 1; i < MESSAGES_QUEUE_SIZE; i++) {
                    newQueue.put(queueJSON.get(i));
                }
                queueJSON = newQueue;
            }
            // Add the new message to the end.
            queueJSON.put(message.toJSON());
            sharedPrefs.edit().putString(subscriptionId, queueJSON.toString()).apply();
        } catch (JSONException e) {
            Log.e(TAG,
                    "Error when parsing the persisted message queue for subscriber:"
                            + subscriptionId + ":" + e.getMessage());
        }
    }

    /**
     * Filters our any messages in |messagesJSON| with the given collpase key. It returns the
     * filtered list.
     */
    private static JSONArray filterMessageBasedOnCollapseKey(JSONArray messages, String collapseKey)
            throws JSONException {
        JSONArray filteredMessages = new JSONArray();
        for (int i = 0; i < messages.length(); i++) {
            JSONObject message = messages.getJSONObject(i);
            if (GCMMessage.peekCollapseKey(message).equals(collapseKey)) {
                Log.i(TAG,
                        "Dropping GCM Message due to collapse key collision. Sender id:"
                                + GCMMessage.peekSenderId(message));
                continue;
            }
            filteredMessages.put(message);
        }
        return filteredMessages;
    }

    /**
     *  Reads messages stored using persistMessage() for |subscriptionId|. No
     *  more than |MESSAGES_QUEUE_SIZE| are returned.
     *  @param subscriptionId The subscription id of the stored messages.
     *  @return The messages stored. Returns an empty list in case of failure.
     */
    public static GCMMessage[] readMessages(String subscriptionId) {
        // TODO(https://crbug.com/882887): Make sure to delete subscription
        // information when the token goes.
        Context context = ContextUtils.getApplicationContext();
        SharedPreferences sharedPrefs =
                context.getSharedPreferences(InstanceIDBridge.PREF_PACKAGE, Context.MODE_PRIVATE);

        // Default is an empty queue if no messages are queued for this subscription.
        String queueString = sharedPrefs.getString(subscriptionId, "[]");
        try {
            JSONArray queueJSON = new JSONArray(queueString);
            List<GCMMessage> messages = new ArrayList<>();
            for (int i = 0; i < queueJSON.length(); i++) {
                try {
                    GCMMessage persistedMessage =
                            GCMMessage.createFromJSON(queueJSON.getJSONObject(i));
                    if (persistedMessage == null) {
                        Log.e(TAG,
                                "Persisted GCM Message is invalid. Sender id:"
                                        + GCMMessage.peekSenderId(queueJSON.getJSONObject(i)));
                        continue;
                    }
                    messages.add(persistedMessage);
                } catch (JSONException e) {
                    Log.e(TAG,
                            "Error when creating a GCMMessage from a JSONObject:" + e.getMessage());
                }
            }
            return messages.toArray(new GCMMessage[messages.size()]);
        } catch (JSONException e) {
            Log.e(TAG,
                    "Error when parsing the persisted message queue for subscriber:"
                            + subscriptionId);
        }
        return new GCMMessage[0];
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

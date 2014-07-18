// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.util.Log;

import com.google.android.gcm.GCMRegistrar;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.content.browser.BrowserStartupController;

import java.util.ArrayList;
import java.util.List;

/**
 * This class is the Java counterpart to the C++ GCMDriverAndroid class.
 * It uses Android's Java GCM APIs to implements GCM registration etc, and
 * sends back GCM messages over JNI.
 *
 * Threading model: all calls to/from C++ happen on the UI thread.
 */
@JNINamespace("gcm")
public class GCMDriver {
    private static final String TAG = "GCMDriver";

    private static final String LAST_GCM_APP_ID_KEY = "last_gcm_app_id";

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
    private void register(final String appId, final String[] senderIds) {
        setLastAppId(appId);
        new AsyncTask<Void, Void, String>() {
            @Override
            protected String doInBackground(Void... voids) {
                try {
                    GCMRegistrar.checkDevice(mContext);
                } catch (UnsupportedOperationException ex) {
                    return ""; // Indicates failure.
                }
                // TODO(johnme): Move checkManifest call to a test instead.
                GCMRegistrar.checkManifest(mContext);
                String existingRegistrationId = GCMRegistrar.getRegistrationId(mContext);
                if (existingRegistrationId.equals("")) {
                    // TODO(johnme): Migrate from GCMRegistrar to GoogleCloudMessaging API, both
                    // here and elsewhere in Chromium.
                    // TODO(johnme): Pass appId to GCM.
                    GCMRegistrar.register(mContext, senderIds);
                    return null; // Indicates pending result.
                } else {
                    Log.i(TAG, "Re-using existing registration ID");
                    return existingRegistrationId;
                }
            }
            @Override
            protected void onPostExecute(String registrationId) {
                if (registrationId == null) {
                    return; // Wait for {@link #onRegisterFinished} to be called.
                }
                nativeOnRegisterFinished(mNativeGCMDriverAndroid, appId, registrationId,
                                         !registrationId.isEmpty());
            }
        }.execute();
    }

    private enum UnregisterResult { SUCCESS, FAILED, PENDING }

    @CalledByNative
    private void unregister(final String appId) {
        new AsyncTask<Void, Void, UnregisterResult>() {
            @Override
            protected UnregisterResult doInBackground(Void... voids) {
                try {
                    GCMRegistrar.checkDevice(mContext);
                } catch (UnsupportedOperationException ex) {
                    return UnregisterResult.FAILED;
                }
                if (!GCMRegistrar.isRegistered(mContext)) {
                    return UnregisterResult.SUCCESS;
                }
                // TODO(johnme): Pass appId to GCM.
                GCMRegistrar.unregister(mContext);
                return UnregisterResult.PENDING;
            }

            @Override
            protected void onPostExecute(UnregisterResult result) {
                if (result == UnregisterResult.PENDING) {
                    return; // Wait for {@link #onUnregisterFinished} to be called.
                }
                nativeOnUnregisterFinished(mNativeGCMDriverAndroid, appId,
                        result == UnregisterResult.SUCCESS);
            }
        }.execute();
    }

    static void onRegisterFinished(String appId, String registrationId) {
        ThreadUtils.assertOnUiThread();
        // TODO(johnme): If this gets called, did it definitely succeed?
        // TODO(johnme): Update registrations cache?
        if (sInstance != null) {
            sInstance.nativeOnRegisterFinished(sInstance.mNativeGCMDriverAndroid, getLastAppId(),
                                               registrationId, true);
        }
    }

    static void onUnregisterFinished(String appId) {
        ThreadUtils.assertOnUiThread();
        // TODO(johnme): If this gets called, did it definitely succeed?
        // TODO(johnme): Update registrations cache?
        if (sInstance != null) {
            sInstance.nativeOnUnregisterFinished(sInstance.mNativeGCMDriverAndroid, getLastAppId(),
                                                 true);
        }
    }

    static void onMessageReceived(Context context, final String appId, final Bundle extras) {
        final String PUSH_API_DATA_KEY = "data";
        if (!extras.containsKey(PUSH_API_DATA_KEY)) {
            // For now on Android only the Push API uses GCMDriver. To avoid double-handling of
            // messages already handled in Java by other implementations of MultiplexingGcmListener,
            // and unnecessarily waking up the browser processes for all existing GCM messages that
            // are received by Chrome on Android, we currently discard messages unless they are
            // destined for the Push API.
            // TODO(johnme): Find a better way of distinguishing messages that should be delivered
            // to native from messages that have already been delivered to Java, for example by
            // refactoring other implementations of MultiplexingGcmListener to instead register with
            // this class, and distinguish them based on appId (which also requires GCM to start
            // sending us the app IDs).
            return;
        }

        // TODO(johnme): Store message and redeliver later if Chrome is killed before delivery.
        ThreadUtils.assertOnUiThread();
        launchNativeThen(context, new Runnable() {
            @Override public void run() {
                final String BUNDLE_SENDER_ID = "from";
                final String BUNDLE_COLLAPSE_KEY = "collapse_key";
                final String BUNDLE_GCMMPLEX = "com.google.ipc.invalidation.gcmmplex.";

                String senderId = extras.getString(BUNDLE_SENDER_ID);
                String collapseKey = extras.getString(BUNDLE_COLLAPSE_KEY);

                List<String> dataKeysAndValues = new ArrayList<String>();
                for (String key : extras.keySet()) {
                    // TODO(johnme): Check there aren't other keys that we need to exclude.
                    if (key == BUNDLE_SENDER_ID || key == BUNDLE_COLLAPSE_KEY ||
                            key.startsWith(BUNDLE_GCMMPLEX))
                        continue;
                    dataKeysAndValues.add(key);
                    dataKeysAndValues.add(extras.getString(key));
                }

                sInstance.nativeOnMessageReceived(sInstance.mNativeGCMDriverAndroid,
                        getLastAppId(), senderId, collapseKey,
                        dataKeysAndValues.toArray(new String[dataKeysAndValues.size()]));
            }
        });
    }

    static void onMessagesDeleted(Context context, final String appId) {
        // TODO(johnme): Store event and redeliver later if Chrome is killed before delivery.
        ThreadUtils.assertOnUiThread();
        launchNativeThen(context, new Runnable() {
            @Override public void run() {
                sInstance.nativeOnMessagesDeleted(sInstance.mNativeGCMDriverAndroid,
                        getLastAppId());
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

    // TODO(johnme): This and setLastAppId are just temporary (crbug.com/350383).
    private static String getLastAppId() {
        SharedPreferences settings = PreferenceManager.getDefaultSharedPreferences(
                sInstance.mContext);
        return settings.getString(LAST_GCM_APP_ID_KEY, "push#unknown_app_id#0");
    }

    private static void setLastAppId(String appId) {
        SharedPreferences settings = PreferenceManager.getDefaultSharedPreferences(
                sInstance.mContext);
        SharedPreferences.Editor editor = settings.edit();
        editor.putString(LAST_GCM_APP_ID_KEY, appId);
        editor.commit();
    }

    private static void launchNativeThen(Context context, Runnable task) {
        if (sInstance != null) {
            task.run();
            return;
        }

        // TODO(johnme): Call ChromeMobileApplication.initCommandLine(context) or
        // ChromeShellApplication.initCommandLine() as appropriate.

        try {
            BrowserStartupController.get(context).startBrowserProcessesSync(false);
            if (sInstance != null) {
                task.run();
            } else {
                Log.e(TAG, "Started browser process, but failed to instantiate GCMDriver.");
            }
        } catch (ProcessInitException e) {
            Log.e(TAG, "Failed to start browser process.", e);
            System.exit(-1);
        }

        // TODO(johnme): Now we should probably exit?
    }
}

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

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.content.app.ContentApplication;
import org.chromium.content.browser.BrowserStartupController;

import java.io.IOException;
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
                // TODO(johnme): Should check if GMS is installed on the device first. Ditto below.
                try {
                    String subtype = appId;
                    GoogleCloudMessagingV2 gcm = new GoogleCloudMessagingV2(mContext);
                    String registrationId = gcm.register(subtype, senderIds);
                    return registrationId;
                } catch (IOException ex) {
                    Log.w(TAG, "GCMv2 registration failed for " + appId, ex);
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
    private void unregister(final String appId) {
        new AsyncTask<Void, Void, Boolean>() {
            @Override
            protected Boolean doInBackground(Void... voids) {
                try {
                    String subtype = appId;
                    GoogleCloudMessagingV2 gcm = new GoogleCloudMessagingV2(mContext);
                    gcm.unregister(subtype);
                    return true;
                } catch (IOException ex) {
                    Log.w(TAG, "GCMv2 unregistration failed for " + appId, ex);
                    return false;
                }
            }

            @Override
            protected void onPostExecute(Boolean success) {
                nativeOnUnregisterFinished(mNativeGCMDriverAndroid, appId, success);
            }
        }.execute();
    }

    static void onMessageReceived(Context context, final String appId, final Bundle extras) {
        // TODO(johnme): Store message and redeliver later if Chrome is killed before delivery.
        ThreadUtils.assertOnUiThread();
        launchNativeThen(context, new Runnable() {
            @Override public void run() {
                final String bundleSubtype = "subtype";
                final String bundleSenderId = "from";
                final String bundleCollapseKey = "collapse_key";
                final String bundleGcmplex = "com.google.ipc.invalidation.gcmmplex.";

                String senderId = extras.getString(bundleSenderId);
                String collapseKey = extras.getString(bundleCollapseKey);

                List<String> dataKeysAndValues = new ArrayList<String>();
                for (String key : extras.keySet()) {
                    // TODO(johnme): Check there aren't other keys that we need to exclude.
                    if (key.equals(bundleSubtype) || key.equals(bundleSenderId) ||
                            key.equals(bundleCollapseKey) || key.startsWith(bundleGcmplex))
                        continue;
                    dataKeysAndValues.add(key);
                    dataKeysAndValues.add(extras.getString(key));
                }

                String guessedAppId = GCMListener.UNKNOWN_APP_ID.equals(appId) ? getLastAppId()
                                                                               : appId;
                sInstance.nativeOnMessageReceived(sInstance.mNativeGCMDriverAndroid,
                        guessedAppId, senderId, collapseKey,
                        dataKeysAndValues.toArray(new String[dataKeysAndValues.size()]));
            }
        });
    }

    static void onMessagesDeleted(Context context, final String appId) {
        // TODO(johnme): Store event and redeliver later if Chrome is killed before delivery.
        ThreadUtils.assertOnUiThread();
        launchNativeThen(context, new Runnable() {
            @Override public void run() {
                String guessedAppId = GCMListener.UNKNOWN_APP_ID.equals(appId) ? getLastAppId()
                                                                               : appId;
                sInstance.nativeOnMessagesDeleted(sInstance.mNativeGCMDriverAndroid, guessedAppId);
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

        ContentApplication.initCommandLine(context);

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

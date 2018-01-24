// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ServiceConnection;
import android.net.Uri;
import android.os.IBinder;
import android.support.v4.content.LocalBroadcastManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.content_public.browser.WebContents;

/**
 * A layer of indirection between CastContentWindowAndroid and CastWebContents(Activity|Service).
 * <p>
 * On builds with DISPLAY_WEB_CONTENTS_IN_SERVICE set to false, it will use CastWebContentsActivity,
 * otherwise, it will use CastWebContentsService.
 */
public class CastWebContentsComponent {
    /**
     * Callback interface for when the associated component is closed or the
     * WebContents is detached.
     */
    public interface OnComponentClosedHandler { void onComponentClosed(); }

    /**
     * Callback interface for passing along keyDown events. This only applies
     * to CastWebContentsActivity, really.
     */
    public interface OnKeyDownHandler { void onKeyDown(int keyCode); }

    private interface Delegate {
        void start(Context context, WebContents webContents);
        void stop(Context context);
    }

    private class ActivityDelegate implements Delegate {
        private static final String TAG = "cr_CastWebContent_AD";
        private boolean mEnableTouchInput;

        public ActivityDelegate(boolean enableTouchInput) {
            mEnableTouchInput = enableTouchInput;
        }

        @Override
        public void start(Context context, WebContents webContents) {
            if (DEBUG) Log.d(TAG, "start: SHOW_WEB_CONTENT in activity");
            startCastActivity(context, webContents, mEnableTouchInput);
        }

        @Override
        public void stop(Context context) {
            if (DEBUG) Log.d(TAG, "Stop CastWebContentsActivity");
            Intent intent = new Intent(CastIntents.ACTION_STOP_WEB_CONTENT);
            intent.putExtra(INTENT_EXTRA_URI, getInstanceUri(mInstanceId).toString());
            if (DEBUG) Log.d(TAG, "stop: send STOP_WEB_CONTENT intent");
            getLocalBroadcastManager().sendBroadcastSync(intent);
        }
    }

    private void startCastActivity(Context context, WebContents webContents, boolean enableTouch) {
        Intent intent =
                new Intent(Intent.ACTION_VIEW, null, context, CastWebContentsActivity.class);
        intent.putExtra(INTENT_EXTRA_URI, getInstanceUri(mInstanceId).toString());
        intent.putExtra(ACTION_EXTRA_WEB_CONTENTS, webContents);
        intent.putExtra(ACTION_EXTRA_TOUCH_INPUT_ENABLED, enableTouch);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        if (DEBUG) Log.d(TAG, "start activity by intent: " + intent);
        context.startActivity(intent);
    }

    private void sendStopWebContentEvent() {
        Intent intent = new Intent(CastIntents.ACTION_STOP_WEB_CONTENT);
        intent.putExtra(INTENT_EXTRA_URI, getInstanceUri(mInstanceId).toString());
        if (DEBUG) Log.d(TAG, "stop: send STOP_WEB_CONTENT intent: " + intent);
        getLocalBroadcastManager().sendBroadcastSync(intent);
    }

    private class FragmentDelegate implements Delegate {
        private static final String TAG = "cr_CastWebContent_FD";
        private boolean mEnableTouchInput;

        public FragmentDelegate(boolean enableTouchInput) {
            mEnableTouchInput = enableTouchInput;
        }

        @Override
        public void start(Context context, WebContents webContents) {
            if (DEBUG) Log.d(TAG, "state: SHOW_WEB_CONTENT in fragment");
            Intent intent = new Intent();
            intent.setAction(CastIntents.ACTION_SHOW_WEB_CONTENT);
            intent.putExtra(INTENT_EXTRA_URI, getInstanceUri(mInstanceId).toString());
            intent.putExtra(ACTION_EXTRA_WEB_CONTENTS, webContents);
            intent.putExtra(ACTION_EXTRA_TOUCH_INPUT_ENABLED, mEnableTouchInput);
            if (!getLocalBroadcastManager().sendBroadcast(intent)) {
                // No intent receiver to handle SHOW_WEB_CONTENT in fragment
                startCastActivity(context, webContents, mEnableTouchInput);
            }
        }

        @Override
        public void stop(Context context) {
            sendStopWebContentEvent();
        }
    }

    private class ServiceDelegate implements Delegate {
        private static final String TAG = "cr_CastWebContent_SD";

        private ServiceConnection mConnection = new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {}

            @Override
            public void onServiceDisconnected(ComponentName name) {
                if (DEBUG) Log.d(TAG, "onServiceDisconnected");

                if (mComponentClosedHandler != null) mComponentClosedHandler.onComponentClosed();
            }
        };

        @Override
        public void start(Context context, WebContents webContents) {
            if (DEBUG) Log.d(TAG, "start");

            Intent intent = new Intent(Intent.ACTION_VIEW, getInstanceUri(mInstanceId), context,
                    CastWebContentsService.class);
            intent.putExtra(ACTION_EXTRA_WEB_CONTENTS, webContents);
            context.bindService(intent, mConnection, Context.BIND_AUTO_CREATE);
        }

        @Override
        public void stop(Context context) {
            if (DEBUG) Log.d(TAG, "stop");

            context.unbindService(mConnection);
        }
    }

    private class Receiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent.getAction().equals(ACTION_ACTIVITY_STOPPED)) {
                if (DEBUG) Log.d(TAG, "onReceive ACTION_ACTIVITY_STOPPED");

                if (mComponentClosedHandler != null) mComponentClosedHandler.onComponentClosed();
            } else if (intent.getAction().equals(ACTION_KEY_EVENT)) {
                if (DEBUG) Log.d(TAG, "onReceive ACTION_KEY_EVENT");

                int keyCode = intent.getIntExtra(ACTION_EXTRA_KEY_CODE, 0);
                if (mKeyDownHandler != null) mKeyDownHandler.onKeyDown(keyCode);
            }
        }
    }

    static final String INTENT_EXTRA_URI = "content_uri";

    static final String ACTION_DATA_SCHEME = "cast";
    static final String ACTION_DATA_AUTHORITY = "webcontents";
    static final String ACTION_EXTRA_WEB_CONTENTS =
            "com.google.android.apps.castshell.intent.extra.WEB_CONTENTS";
    static final String ACTION_EXTRA_TOUCH_INPUT_ENABLED =
            "com.google.android.apps.castshell.intent.extra.ENABLE_TOUCH";

    private static final String TAG = "cr_CastWebComponent";
    private static final boolean DEBUG = true;

    private static final String ACTION_EXTRA_KEY_CODE =
            "com.google.android.apps.castshell.intent.extra.KEY_CODE";
    private static final String ACTION_KEY_EVENT =
            "com.google.android.apps.castshell.intent.action.KEY_EVENT";
    private static final String ACTION_ACTIVITY_STOPPED =
            "com.google.android.apps.castshell.intent.action.ACTIVITY_STOPPED";

    private final Delegate mDelegate;
    private final OnComponentClosedHandler mComponentClosedHandler;
    private final OnKeyDownHandler mKeyDownHandler;
    private Receiver mReceiver;
    private String mInstanceId;
    private boolean mStarted = false;

    public CastWebContentsComponent(String instanceId,
            OnComponentClosedHandler onComponentClosedHandler, OnKeyDownHandler onKeyDownHandler,
            boolean isHeadless, boolean enableTouchInput) {
        mComponentClosedHandler = onComponentClosedHandler;
        mKeyDownHandler = onKeyDownHandler;
        mInstanceId = instanceId;
        if (BuildConfig.DISPLAY_WEB_CONTENTS_IN_SERVICE || isHeadless) {
            if (DEBUG) Log.d(TAG, "Creating service delegate...");
            mDelegate = new ServiceDelegate();
        } else if (BuildConfig.ENABLE_CAST_FRAGMENT) {
            if (DEBUG) Log.d(TAG, "Creating fragment delegate...");
            mDelegate = new FragmentDelegate(enableTouchInput);
        } else {
            if (DEBUG) Log.d(TAG, "Creating activity delegate...");
            mDelegate = new ActivityDelegate(enableTouchInput);
        }
    }

    public void start(Context context, WebContents webContents) {
        if (DEBUG) Log.d(TAG, "start with delegate: " + mDelegate.getClass().getSimpleName());

        Uri instanceUri = getInstanceUri(mInstanceId);
        mReceiver = new Receiver();
        IntentFilter filter = new IntentFilter();
        filter.addAction(ACTION_ACTIVITY_STOPPED);
        filter.addAction(ACTION_KEY_EVENT);

        getLocalBroadcastManager().registerReceiver(mReceiver, filter);

        mDelegate.start(context, webContents);
        mStarted = true;
    }

    public void stop(Context context) {
        if (DEBUG) Log.d(TAG, "stop");
        if (mStarted) {
            getLocalBroadcastManager().unregisterReceiver(mReceiver);
            mDelegate.stop(context);
            mStarted = false;
        }
    }

    public static void onComponentClosed(Context context, String instanceId) {
        if (DEBUG) Log.d(TAG, "onComponentClosed");

        Intent intent = new Intent(ACTION_ACTIVITY_STOPPED, getInstanceUri(instanceId));
        getLocalBroadcastManager().sendBroadcastSync(intent);
    }

    public static void onKeyDown(Context context, String instanceId, int keyCode) {
        if (DEBUG) Log.d(TAG, "onKeyDown");

        Intent intent = new Intent(ACTION_KEY_EVENT, getInstanceUri(instanceId));
        intent.putExtra(ACTION_EXTRA_KEY_CODE, keyCode);
        getLocalBroadcastManager().sendBroadcastSync(intent);
    }

    public static Uri getInstanceUri(String instanceId) {
        Uri instanceUri = new Uri.Builder()
                                  .scheme(ACTION_DATA_SCHEME)
                                  .authority(ACTION_DATA_AUTHORITY)
                                  .path(instanceId)
                                  .build();
        return instanceUri;
    }

    private static LocalBroadcastManager getLocalBroadcastManager() {
        return LocalBroadcastManager.getInstance(ContextUtils.getApplicationContext());
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.annotation.SuppressLint;
import android.app.ActivityManager;
import android.app.PendingIntent;
import android.app.PictureInPictureParams;
import android.app.RemoteAction;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.Configuration;
import android.graphics.drawable.Icon;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.MediaSession;
import org.chromium.content_public.browser.MediaSessionObserver;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;

/**
 * A picture in picture activity which get created when requesting
 * PiP from web API. The activity will connect to web API through
 * OverlayWindowAndroid.
 */
public class PictureInPictureActivity extends AsyncInitializationActivity {
    private static final String ACTION_PLAY =
            "org.chromium.chrome.browser.media.PictureInPictureActivity.Play";

    private static long sNativeOverlayWindowAndroid;
    private static Tab sInitiatorTab;
    private static int sInitiatorTabTaskID;
    private static InitiatorTabObserver sTabObserver;

    private MediaSessionObserver mMediaSessionObserver;

    private BroadcastReceiver mMediaSessionReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (sNativeOverlayWindowAndroid == 0) return;

            if (intent.getAction() == null || !intent.getAction().equals(ACTION_PLAY)) return;

            PictureInPictureActivityJni.get().play(sNativeOverlayWindowAndroid);
        }
    };

    private static class InitiatorTabObserver extends EmptyTabObserver {
        private enum Status { OK, DESTROYED }
        private PictureInPictureActivity mActivity;
        private Status mStatus;

        InitiatorTabObserver() {
            mStatus = Status.OK;
        }

        public void setActivity(PictureInPictureActivity activity) {
            mActivity = activity;
        }

        public Status getStatus() {
            return mStatus;
        }

        @Override
        public void onDestroyed(Tab tab) {
            if (tab.isClosing() || !isInitiatorTabAlive()) {
                mStatus = Status.DESTROYED;
                if (mActivity != null) mActivity.finish();
            }
        }

        @Override
        public void onCrash(Tab tab) {
            mStatus = Status.DESTROYED;
            if (mActivity != null) mActivity.finish();
        }
    }

    @Override
    protected void triggerLayoutInflation() {
        onInitialLayoutInflationComplete();
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return true;
    }

    @Override
    public void onStart() {
        super.onStart();

        // Finish the activity if OverlayWindowAndroid has already been destroyed
        // or InitiatorTab has been destroyed by user or crashed.
        if (sNativeOverlayWindowAndroid == 0
                || sTabObserver.getStatus() == InitiatorTabObserver.Status.DESTROYED) {
            this.finish();
            return;
        }

        sTabObserver.setActivity(this);

        registerReceiver(mMediaSessionReceiver, new IntentFilter(ACTION_PLAY));

        PictureInPictureActivityJni.get().onActivityStart(
                sNativeOverlayWindowAndroid, this, getWindowAndroid());

        // Add an observer to refresh the Picture-in-Picture params if the media
        // session state changes.
        MediaSession mediaSession = MediaSession.fromWebContents(sInitiatorTab.getWebContents());
        mMediaSessionObserver = new MediaSessionObserver(mediaSession) {
            @Override
            public void mediaSessionStateChanged(boolean isControllable, boolean isSuspended) {
                setPictureInPictureParams(getPictureInPictureParams());
            }
        };

        enterPictureInPictureMode(getPictureInPictureParams());
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        sNativeOverlayWindowAndroid = 0;
        sInitiatorTab.removeObserver(sTabObserver);
        sInitiatorTab = null;
        sTabObserver = null;

        if (mMediaSessionObserver != null) {
            mMediaSessionObserver.stopObserving();
            mMediaSessionObserver = null;
        }

        unregisterReceiver(mMediaSessionReceiver);
    }

    @Override
    public void onPictureInPictureModeChanged(
            boolean isInPictureInPictureMode, Configuration newConfig) {
        if (!isInPictureInPictureMode) this.finish();
    }

    @Override
    protected ActivityWindowAndroid createWindowAndroid() {
        return new ActivityWindowAndroid(this);
    }

    @SuppressLint("NewApi")
    private static boolean isInitiatorTabAlive() {
        ActivityManager activityManager =
                (ActivityManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.ACTIVITY_SERVICE);
        for (ActivityManager.AppTask appTask : activityManager.getAppTasks()) {
            if (appTask.getTaskInfo().id == sInitiatorTabTaskID) return true;
        }

        return false;
    }

    @CalledByNative
    private void close() {
        this.finish();
    }

    @SuppressLint("NewApi")
    private PictureInPictureParams getPictureInPictureParams() {
        ArrayList<RemoteAction> actions = new ArrayList<>();

        // If the associated media session is not controllable then we should
        // place a play button in the Picture-in-Picture window that will
        // trigger playback.
        if (mMediaSessionObserver != null
                && !mMediaSessionObserver.getMediaSession().isControllable()) {
            PendingIntent pendingIntent = PendingIntent.getBroadcast(
                    getApplicationContext(), 0, new Intent(ACTION_PLAY), 0);

            actions.add(new RemoteAction(Icon.createWithResource(getApplicationContext(),
                                                 R.drawable.ic_play_arrow_white_36dp),
                    getApplicationContext().getResources().getText(R.string.accessibility_play), "",
                    pendingIntent));
        }

        return new PictureInPictureParams.Builder().setActions(actions).build();
    }

    @CalledByNative
    private static void createActivity(long nativeOverlayWindowAndroid, Object initiatorTab) {
        Context context = ContextUtils.getApplicationContext();
        Intent intent = new Intent(context, PictureInPictureActivity.class);

        // Dissociate OverlayWindowAndroid if there is one already.
        if (sNativeOverlayWindowAndroid != 0)
            PictureInPictureActivityJni.get().destroy(sNativeOverlayWindowAndroid);

        sNativeOverlayWindowAndroid = nativeOverlayWindowAndroid;
        sInitiatorTab = (Tab) initiatorTab;
        sInitiatorTabTaskID = sInitiatorTab.getActivity().getTaskId();

        sTabObserver = new InitiatorTabObserver();
        sInitiatorTab.addObserver(sTabObserver);

        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
    }

    @CalledByNative
    private static void onWindowDestroyed(long nativeOverlayWindowAndroid) {
        if (sNativeOverlayWindowAndroid != nativeOverlayWindowAndroid) return;

        sNativeOverlayWindowAndroid = 0;
    }

    @NativeMethods
    interface Natives {
        void onActivityStart(long nativeOverlayWindowAndroid, PictureInPictureActivity self,
                WindowAndroid window);

        void destroy(long nativeOverlayWindowAndroid);

        void play(long nativeOverlayWindowAndroid);
    }
}

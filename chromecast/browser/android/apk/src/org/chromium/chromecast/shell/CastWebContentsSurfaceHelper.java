// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Color;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Handler;
import android.support.v4.content.LocalBroadcastManager;
import android.widget.FrameLayout;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content.browser.ActivityContentVideoViewEmbedder;
import org.chromium.content.browser.ContentVideoViewEmbedder;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.ContentViewRenderView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

/**
 * A util class for CastWebContentsActivity and CastWebContentsFragment to show
 * WebContent on its views.
 * <p>
 * This class is to help the activity or fragment class to work with CastContentWindowAndroid,
 * which will start a new instance of activity or fragment. If the CastContentWindowAndroid is
 * destroyed, CastWebContentsActivity or CastWebContentsFragment should be stopped.
 * Similarily,  CastWebContentsActivity or CastWebContentsFragment is stopped, eg.
 * CastWebContentsFragment is removed from a activity or the activity holding it
 * is destroyed, or CastWebContentsActivity is closed, CastContentWindowAndroid should be
 * notified by intent.
 */
@JNINamespace("chromecast::shell")
class CastWebContentsSurfaceHelper {
    private static final String TAG = "cr_CastWebContents";

    private static final int TEARDOWN_GRACE_PERIOD_TIMEOUT_MILLIS = 300;

    private final Activity mHostActivity;
    private final boolean mShowInFragment;
    private final Handler mHandler;
    private final FrameLayout mCastWebContentsLayout;
    private final CastAudioManager mAudioManager;

    private Uri mUri;
    private String mInstanceId;
    private BroadcastReceiver mWindowDestroyedBroadcastReceiver;
    private BroadcastReceiver mScreenOffBroadcastReceiver;
    private BroadcastReceiver mInternalStopReceiver;
    private ContentViewRenderView mContentViewRenderView;
    private WindowAndroid mWindow;
    private ContentViewCore mContentViewCore;
    private ContentView mContentView;

    // TODO(vincentli) interrupt touch event from Fragment's root view when it's false.
    private boolean mTouchInputEnabled = false;

    /**
     * @param hostActivity Activity hosts the view showing WebContents
     * @param castWebContentsLayout view group to add ContentViewRenderView and ContentView
     * @param showInFragment true if the cast web view is hosted by a CastWebContentsFragment
     */
    CastWebContentsSurfaceHelper(
            Activity hostActivity, FrameLayout castWebContentsLayout, boolean showInFragment) {
        mHostActivity = hostActivity;
        mShowInFragment = showInFragment;
        mCastWebContentsLayout = castWebContentsLayout;
        mHandler = new Handler();
        mAudioManager = CastAudioManager.getAudioManager(getActivity());
    }

    void onNewWebContents(
            final Uri uri, final WebContents webContents, final boolean touchInputEnabled) {
        if (webContents == null) {
            Log.e(TAG, "Received null WebContents in bundle.");
            maybeFinishLater();
            return;
        }

        mTouchInputEnabled = touchInputEnabled;

        Log.d(TAG, "content_uri=" + uri);
        mUri = uri;
        mInstanceId = uri.getPath();

        // Whenever our app is visible, volume controls should modify the music stream.
        // For more information read:
        // http://developer.android.com/training/managing-audio/volume-playback.html
        mHostActivity.setVolumeControlStream(AudioManager.STREAM_MUSIC);

        if (mWindowDestroyedBroadcastReceiver != null) {
            getLocalBroadcastManager().unregisterReceiver(mWindowDestroyedBroadcastReceiver);
        }

        if (mScreenOffBroadcastReceiver != null) {
            getLocalBroadcastManager().unregisterReceiver(mScreenOffBroadcastReceiver);
        }

        mScreenOffBroadcastReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                detachWebContentsIfAny();
                maybeFinishLater();
            }
        };

        IntentFilter screenOffIntentFilter = new IntentFilter();
        screenOffIntentFilter.addAction(CastIntents.ACTION_SCREEN_OFF);
        getLocalBroadcastManager().registerReceiver(
                mScreenOffBroadcastReceiver, screenOffIntentFilter);

        if (mInternalStopReceiver != null) {
            getLocalBroadcastManager().unregisterReceiver(mWindowDestroyedBroadcastReceiver);
        }
        mInternalStopReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                Log.d(TAG, "Intent action=" + intent.getAction());
                if (mUri == null
                        || !mUri.toString().equals(intent.getStringExtra(
                                   CastWebContentsComponent.INTENT_EXTRA_URI))) {
                    Log.d(TAG,
                            "Current URI=" + mUri + "; intent URI="
                                    + intent.getStringExtra(
                                              CastWebContentsComponent.INTENT_EXTRA_URI));
                    return;
                }
                detachWebContentsIfAny();
                maybeFinishLater();
            }
        };
        IntentFilter internalStopReceiverFilter = new IntentFilter();
        internalStopReceiverFilter.addAction(CastIntents.ACTION_STOP_WEB_CONTENT);
        getLocalBroadcastManager().registerReceiver(
                mInternalStopReceiver, internalStopReceiverFilter);

        showWebContents(webContents);
    }

    // Closes this activity if a new WebContents is not being displayed.
    private void maybeFinishLater() {
        Log.d(TAG, "maybeFinishLater");
        final String currentInstanceId = mInstanceId;
        mHandler.postDelayed(new Runnable() {
            @Override
            public void run() {
                if (currentInstanceId.equals(mInstanceId)) {
                    Log.d(TAG, "Finishing.");
                    if (mShowInFragment) {
                        Intent in = new Intent();
                        in.setAction(CastIntents.ACTION_ON_WEB_CONTENT_STOPPED);
                        in.putExtra(CastWebContentsComponent.INTENT_EXTRA_URI, mUri.toString());
                        getLocalBroadcastManager().sendBroadcastSync(in);
                    } else {
                        mHostActivity.finish();
                    }
                }
            }
        }, TEARDOWN_GRACE_PERIOD_TIMEOUT_MILLIS);
    }

    private Activity getActivity() {
        return mHostActivity;
    }

    // Sets webContents to be the currently displayed webContents.
    private void showWebContents(WebContents webContents) {
        Log.d(TAG, "showWebContents");

        detachWebContentsIfAny();

        // Set ContentVideoViewEmbedder to allow video playback.
        nativeSetContentVideoViewEmbedder(
                webContents, new ActivityContentVideoViewEmbedder(getActivity()));

        mWindow = new WindowAndroid(getActivity());
        mContentViewRenderView = new ContentViewRenderView(getActivity()) {
            @Override
            protected void onReadyToRender() {
                setOverlayVideoMode(true);
            }
        };
        mContentViewRenderView.onNativeLibraryLoaded(mWindow);
        // Setting the background color to black avoids rendering a white splash screen
        // before the players are loaded. See https://crbug/307113 for details.
        mContentViewRenderView.setSurfaceViewBackgroundColor(Color.BLACK);

        mCastWebContentsLayout.addView(mContentViewRenderView,
                new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));

        // TODO(derekjchow): productVersion
        mContentViewCore = ContentViewCore.create(getActivity().getApplicationContext(), "");
        mContentView = ContentView.createContentView(
                getActivity().getApplicationContext(), mContentViewCore);
        mContentViewCore.initialize(ViewAndroidDelegate.createBasicDelegate(mContentView),
                mContentView, webContents, mWindow);
        // Enable display of current webContents.
        mContentViewCore.onShow();
        mCastWebContentsLayout.addView(mContentView,
                new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));
        mContentView.setFocusable(true);
        mContentView.requestFocus();
        mContentViewRenderView.setCurrentContentViewCore(mContentViewCore);
    }

    // Remove the currently displayed webContents. no-op if nothing is being displayed.
    void detachWebContentsIfAny() {
        Log.d(TAG, "Detach web contents if any.");
        if (mContentView != null) {
            mCastWebContentsLayout.removeView(mContentView);
            mCastWebContentsLayout.removeView(mContentViewRenderView);
            mContentViewCore.destroy();
            mContentViewRenderView.destroy();
            mWindow.destroy();
            mContentView = null;
            mContentViewCore = null;
            mContentViewRenderView = null;
            mWindow = null;
            CastWebContentsComponent.onComponentClosed(getActivity(), mInstanceId);
            Log.d(TAG, "Detach web contents done.");
        }
    }

    void onPause() {
        // Release the audio focus. Note that releasing audio focus does not stop audio playback,
        // it just notifies the framework that this activity has stopped playing audio.
        if (mAudioManager.abandonAudioFocus(null) != AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
            Log.e(TAG, "Failed to abandon audio focus");
        }

        if (mContentViewCore != null) {
            mContentViewCore.onPause();
        }

        releaseStreamMuteIfNecessary();
    }

    @SuppressWarnings("deprecation")
    private void releaseStreamMuteIfNecessary() {
        AudioManager audioManager = mAudioManager.getInternal();
        boolean isMuted = false;
        try {
            isMuted = (Boolean) audioManager.getClass()
                              .getMethod("isStreamMute", int.class)
                              .invoke(audioManager, AudioManager.STREAM_MUSIC);
        } catch (Exception e) {
            Log.e(TAG, "Cannot call AudioManager.isStreamMute().", e);
        }

        if (isMuted) {
            // Note: this is a no-op on fixed-volume devices.
            audioManager.setStreamMute(AudioManager.STREAM_MUSIC, false);
        }
    }

    void onResume() {
        if (mAudioManager.requestAudioFocus(
                    null, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN)
                != AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
            Log.e(TAG, "Failed to obtain audio focus");
        }
        if (mContentViewCore != null) {
            mContentViewCore.onResume();
        }
    }

    // Destroys all resources. After calling this method, this object must be dropped.
    void onDestroy() {
        detachWebContentsIfAny();

        if (mWindowDestroyedBroadcastReceiver != null) {
            getLocalBroadcastManager().unregisterReceiver(mWindowDestroyedBroadcastReceiver);
        }

        if (mScreenOffBroadcastReceiver != null) {
            getLocalBroadcastManager().unregisterReceiver(mScreenOffBroadcastReceiver);
        }

        if (mInternalStopReceiver != null) {
            getLocalBroadcastManager().unregisterReceiver(mInternalStopReceiver);
        }
    }

    String getInstanceId() {
        return mInstanceId;
    }

    boolean isTouchInputEnabled() {
        return mTouchInputEnabled;
    }

    private LocalBroadcastManager getLocalBroadcastManager() {
        return LocalBroadcastManager.getInstance(ContextUtils.getApplicationContext());
    }

    private native void nativeSetContentVideoViewEmbedder(
            WebContents webContents, ContentVideoViewEmbedder embedder);
}

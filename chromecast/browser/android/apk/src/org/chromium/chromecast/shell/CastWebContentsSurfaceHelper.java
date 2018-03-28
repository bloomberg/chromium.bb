// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Color;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Handler;
import android.widget.FrameLayout;

import org.chromium.base.Log;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chromecast.base.CastSwitches;
import org.chromium.chromecast.base.Controller;
import org.chromium.chromecast.base.ScopeFactories;
import org.chromium.chromecast.base.Unit;
import org.chromium.components.content_view.ContentView;
import org.chromium.content.browser.ActivityContentVideoViewEmbedder;
import org.chromium.content.browser.ContentVideoViewEmbedder;
import org.chromium.content.browser.ContentViewRenderView;
import org.chromium.content_public.browser.ContentViewCore;
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

    private final Controller<Unit> mResumedState = new Controller<>();
    private final Controller<Uri> mHasUriState = new Controller<>();

    private final Activity mHostActivity;
    private final boolean mShowInFragment;
    private final Handler mHandler;
    private final FrameLayout mCastWebContentsLayout;

    private Uri mUri;
    private String mInstanceId;
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
        mCastWebContentsLayout.setBackgroundColor(CastSwitches.getSwitchValueColor(
                CastSwitches.CAST_APP_BACKGROUND_COLOR, Color.BLACK));
        mHandler = new Handler();

        // Receive broadcasts indicating the screen turned off while we have active WebContents.
        mHasUriState.watch(() -> {
            IntentFilter filter = new IntentFilter();
            filter.addAction(CastIntents.ACTION_SCREEN_OFF);
            return new LocalBroadcastReceiverScope(filter, (Intent intent) -> {
                detachWebContentsIfAny();
                maybeFinishLater();
            });
        });

        // Receive broadcasts requesting to tear down this app while we have a valid URI.
        mHasUriState.watch((Uri uri) -> {
            IntentFilter filter = new IntentFilter();
            filter.addAction(CastIntents.ACTION_STOP_WEB_CONTENT);
            return new LocalBroadcastReceiverScope(filter, (Intent intent) -> {
                String intentUri = CastWebContentsIntentUtils.getUriString(intent);
                Log.d(TAG, "Intent action=" + intent.getAction() + "; URI=" + intentUri);
                if (!uri.toString().equals(intentUri)) {
                    Log.d(TAG, "Current URI=" + mUri + "; intent URI=" + intentUri);
                    return;
                }
                detachWebContentsIfAny();
                maybeFinishLater();
            });
        });

        // Receive broadcasts indicating that touch input should be enabled.
        // TODO(yyzhong) Handle this intent in an external activity hosting a cast fragment as
        // well.
        mHasUriState.watch((Uri uri) -> {
            IntentFilter filter = new IntentFilter();
            filter.addAction(CastWebContentsIntentUtils.ACTION_ENABLE_TOUCH_INPUT);
            return new LocalBroadcastReceiverScope(filter, (Intent intent) -> {
                String intentUri = CastWebContentsIntentUtils.getUriString(intent);
                Log.d(TAG, "Intent action=" + intent.getAction() + "; URI=" + intentUri);
                if (!uri.toString().equals(intentUri)) {
                    Log.d(TAG, "Current URI=" + mUri + "; intent URI=" + intentUri);
                    return;
                }
                mTouchInputEnabled = CastWebContentsIntentUtils.isTouchable(intent);
            });
        });

        mResumedState.watch(() -> {
            if (mContentViewCore != null) {
                mContentViewCore.onResume();
            }
            return () -> {
                if (mContentViewCore != null) {
                    mContentViewCore.onPause();
                }
            };
        });
        // On pause events, unmute so that that other activities can control the mute state on L.
        mResumedState.watch(ScopeFactories.onExit(() -> releaseStreamMuteIfNecessary()));
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

        mHasUriState.set(mUri);

        showWebContents(webContents);
    }

    // Closes this activity if a new WebContents is not being displayed.
    private void maybeFinishLater() {
        Log.d(TAG, "maybeFinishLater: " + mUri);
        final String currentInstanceId = mInstanceId;
        mHandler.postDelayed(new Runnable() {
            @Override
            public void run() {
                if (currentInstanceId != null && currentInstanceId.equals(mInstanceId)) {
                    if (mShowInFragment) {
                        Log.d(TAG, "Sending intent: ON_WEB_CONTENT_STOPPED: URI=" + mUri);
                        CastWebContentsIntentUtils.getLocalBroadcastManager().sendBroadcastSync(
                                CastWebContentsIntentUtils.onWebContentStopped(mUri));
                    } else {
                        Log.d(TAG, "Finishing cast content activity of URI:" + mUri);
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
        Log.d(TAG, "showWebContents: " + mUri);

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
        // Setting the background color avoids rendering a white splash screen
        // before the players are loaded. See https://crbug/307113 for details.
        mContentViewRenderView.setSurfaceViewBackgroundColor(CastSwitches.getSwitchValueColor(
                CastSwitches.CAST_APP_BACKGROUND_COLOR, Color.BLACK));

        mCastWebContentsLayout.addView(mContentViewRenderView,
                new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));

        Context context = getActivity().getApplicationContext();
        mContentView = ContentView.createContentView(context, webContents);
        // TODO(derekjchow): productVersion
        mContentViewCore = ContentViewCore.create(context, "", webContents,
                ViewAndroidDelegate.createBasicDelegate(mContentView), mContentView, mWindow);
        // Enable display of current webContents.
        webContents.onShow();
        mCastWebContentsLayout.addView(mContentView,
                new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));
        mContentView.setFocusable(true);
        mContentView.requestFocus();
        mContentViewRenderView.setCurrentWebContents(webContents);
    }

    // Remove the currently displayed webContents. no-op if nothing is being displayed.
    private void detachWebContentsIfAny() {
        Log.d(TAG, "Maybe detach web contents if any: " + mUri);
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
            CastWebContentsComponent.onComponentClosed(mInstanceId);
            Log.d(TAG, "Detach web contents done: " + mUri);
        }
    }

    void onPause() {
        Log.d(TAG, "onPause: " + mUri);
        mResumedState.reset();
    }

    @SuppressWarnings("deprecation")
    private void releaseStreamMuteIfNecessary() {
        AudioManager audioManager = CastAudioManager.getAudioManager(getActivity()).getInternal();
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
        Log.d(TAG, "onResume: " + mUri);
        mResumedState.set(Unit.unit());
    }

    // Destroys all resources. After calling this method, this object must be dropped.
    void onDestroy() {
        Log.d(TAG, "onDestroy: " + mUri);
        detachWebContentsIfAny();
        mHasUriState.reset();
    }

    String getInstanceId() {
        return mInstanceId;
    }

    boolean isTouchInputEnabled() {
        return mTouchInputEnabled;
    }

    private native void nativeSetContentVideoViewEmbedder(
            WebContents webContents, ContentVideoViewEmbedder embedder);
}

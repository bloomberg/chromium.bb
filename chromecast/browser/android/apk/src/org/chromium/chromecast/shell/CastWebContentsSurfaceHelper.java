// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.Intent;
import android.content.IntentFilter;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;

import org.chromium.base.Consumer;
import org.chromium.base.Log;
import org.chromium.base.annotations.RemovableInRelease;
import org.chromium.chromecast.base.Both;
import org.chromium.chromecast.base.Controller;
import org.chromium.chromecast.base.Observable;
import org.chromium.chromecast.base.Observer;
import org.chromium.chromecast.base.Observers;
import org.chromium.chromecast.base.Unit;
import org.chromium.content.browser.MediaSessionImpl;
import org.chromium.content_public.browser.WebContents;

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
class CastWebContentsSurfaceHelper {
    private static final String TAG = "CastWebContents";

    private static final int TEARDOWN_GRACE_PERIOD_TIMEOUT_MILLIS = 300;

    // Activated between constructor and onDestroy().
    private final Controller<Unit> mCreatedState = new Controller<>();
    // Activated when we have WebContents to display.
    private final Controller<StartParams> mStartParamsState = new Controller<>();

    private String mSessionId;
    private MediaSessionGetter mMediaSessionGetter;

    // TODO(vincentli) interrupt touch event from Fragment's root view when it's false.
    private boolean mTouchInputEnabled;

    public static class StartParams {
        public final Uri uri;
        public final WebContents webContents;
        public final boolean isRemoteControlMode;
        public final boolean touchInputEnabled;

        public StartParams(Uri uri, WebContents webContents, boolean isRemoteControlMode,
                boolean touchInputEnabled) {
            this.uri = uri;
            this.webContents = webContents;
            this.isRemoteControlMode = isRemoteControlMode;
            this.touchInputEnabled = touchInputEnabled;
        }

        @Override
        public boolean equals(Object other) {
            if (other instanceof StartParams) {
                StartParams that = (StartParams) other;
                return this.uri.equals(that.uri) && this.webContents.equals(that.webContents)
                        && this.isRemoteControlMode == that.isRemoteControlMode
                        && this.touchInputEnabled == that.touchInputEnabled;
            }
            return false;
        }

        public static StartParams fromBundle(Bundle bundle) {
            final String uriString = CastWebContentsIntentUtils.getUriString(bundle);
            if (uriString == null) {
                Log.i(TAG, "Intent without uri received!");
                return null;
            }
            final Uri uri = Uri.parse(uriString);
            if (uri == null) {
                Log.i(TAG, "Invalid URI string: %s", uriString);
                return null;
            }
            bundle.setClassLoader(WebContents.class.getClassLoader());
            final WebContents webContents = CastWebContentsIntentUtils.getWebContents(bundle);
            if (webContents == null) {
                Log.e(TAG, "Received null WebContents in bundle.");
                return null;
            }

            final boolean isRemoteControlMode =
                    CastWebContentsIntentUtils.isRemoteControlMode(bundle);
            final boolean touchInputEnabled = CastWebContentsIntentUtils.isTouchable(bundle);
            return new StartParams(uri, webContents, isRemoteControlMode, touchInputEnabled);
        }
    }

    /**
     * @param webContentsView A Observer that displays incoming WebContents.
     * @param finishCallback Invoked to tell host to finish.
     */
    CastWebContentsSurfaceHelper(
            Observer<WebContents> webContentsView, Consumer<Uri> finishCallback) {
        Handler handler = new Handler();

        mMediaSessionGetter =
                (WebContents webContents) -> MediaSessionImpl.fromWebContents(webContents);

        Observable<Uri> uriState = mStartParamsState.map(params -> params.uri);
        Controller<WebContents> webContentsState = new Controller<>();
        mStartParamsState.map(params -> params.webContents)
                .subscribe(Observers.onEnter(webContentsState::set));
        mCreatedState.subscribe(Observers.onExit(x -> webContentsState.reset()));

        // Receive broadcasts indicating the screen turned off while we have active WebContents.
        uriState.subscribe((Uri uri) -> {
            IntentFilter filter = new IntentFilter();
            filter.addAction(CastIntents.ACTION_SCREEN_OFF);
            return new LocalBroadcastReceiverScope(filter, (Intent intent) -> {
                mStartParamsState.reset();
                webContentsState.reset();
                maybeFinishLater(handler, () -> finishCallback.accept(uri));
            });
        });

        // Receive broadcasts requesting to tear down this app while we have a valid URI.
        uriState.subscribe((Uri uri) -> {
            IntentFilter filter = new IntentFilter();
            filter.addAction(CastIntents.ACTION_STOP_WEB_CONTENT);
            return new LocalBroadcastReceiverScope(filter, (Intent intent) -> {
                String intentUri = CastWebContentsIntentUtils.getUriString(intent);
                Log.d(TAG, "Intent action=" + intent.getAction() + "; URI=" + intentUri);
                if (!uri.toString().equals(intentUri)) {
                    Log.d(TAG, "Current URI=" + uri + "; intent URI=" + intentUri);
                    return;
                }
                mStartParamsState.reset();
                webContentsState.reset();
                maybeFinishLater(handler, () -> finishCallback.accept(uri));
            });
        });

        // Receive broadcasts indicating that touch input should be enabled.
        // TODO(yyzhong) Handle this intent in an external activity hosting a cast fragment as well.
        uriState.subscribe((Uri uri) -> {
            IntentFilter filter = new IntentFilter();
            filter.addAction(CastWebContentsIntentUtils.ACTION_ENABLE_TOUCH_INPUT);
            return new LocalBroadcastReceiverScope(filter, (Intent intent) -> {
                String intentUri = CastWebContentsIntentUtils.getUriString(intent);
                Log.d(TAG, "Intent action=" + intent.getAction() + "; URI=" + intentUri);
                if (!uri.toString().equals(intentUri)) {
                    Log.d(TAG, "Current URI=" + uri + "; intent URI=" + intentUri);
                    return;
                }
                mTouchInputEnabled = CastWebContentsIntentUtils.isTouchable(intent);
            });
        });

        // webContentsView is responsible for displaying each new WebContents.
        webContentsState.subscribe(webContentsView);

        // Take audio focus when receiving new WebContents if not the remote control app.
        mStartParamsState.filter(params -> !params.isRemoteControlMode)
                .map(params -> mMediaSessionGetter.get(params.webContents))
                .subscribe(Observers.onEnter(MediaSessionImpl::requestSystemAudioFocus));

        // Miscellaneous actions responding to WebContents lifecycle.
        webContentsState.subscribe((WebContents webContents) -> {
            // Notify CastWebContentsComponent when closed.
            return () -> CastWebContentsComponent.onComponentClosed(mSessionId);
        });

        // When onDestroy() is called after onNewStartParams(), log and reset StartParams states.
        uriState.andThen(Observable.not(mCreatedState))
                .map(Both::getFirst)
                .subscribe(Observers.onEnter((Uri uri) -> {
                    Log.d(TAG, "onDestroy: " + uri);
                    mStartParamsState.reset();
                }));

        // Cache relevant fields from StartParams in instance variables.
        mStartParamsState.subscribe(Observers.onEnter(params -> {
            mTouchInputEnabled = params.touchInputEnabled;
            mSessionId = params.uri.getPath();
        }));

        mCreatedState.set(Unit.unit());
    }

    void onNewStartParams(final StartParams params) {
        Log.d(TAG, "onNewStartParams: content_uri=" + params.uri);
        mStartParamsState.set(params);
    }

    // Closes this activity if a new WebContents is not being displayed.
    private void maybeFinishLater(Handler handler, Runnable callback) {
        final String currentSessionId = mSessionId;
        handler.postDelayed(() -> {
            if (currentSessionId != null && currentSessionId.equals(mSessionId)) {
                callback.run();
            }
        }, TEARDOWN_GRACE_PERIOD_TIMEOUT_MILLIS);
    }

    // Destroys all resources. After calling this method, this object must be dropped.
    void onDestroy() {
        mCreatedState.reset();
    }

    String getSessionId() {
        return mSessionId;
    }

    boolean isTouchInputEnabled() {
        return mTouchInputEnabled;
    }

    @RemovableInRelease
    void setMediaSessionGetterForTesting(MediaSessionGetter mediaSessionGetter) {
        mMediaSessionGetter = mediaSessionGetter;
    }

    interface MediaSessionGetter {
        MediaSessionImpl get(WebContents webContents);
    }
}

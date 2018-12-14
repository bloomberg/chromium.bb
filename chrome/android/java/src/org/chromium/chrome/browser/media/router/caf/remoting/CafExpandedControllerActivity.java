// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.caf.remoting;

import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.support.v4.app.FragmentActivity;
import android.support.v7.app.MediaRouteButton;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.media.router.caf.BaseSessionController;
import org.chromium.chrome.browser.metrics.MediaNotificationUma;
import org.chromium.third_party.android.media.MediaController;

/**
 * The activity that's opened by clicking the video flinging (casting) notification.
 *
 * TODO(aberent): Refactor to merge some common logic with {@link CastNotificationControl}.
 */
public class CafExpandedControllerActivity
        extends FragmentActivity implements BaseSessionController.Callback {
    private Handler mHandler;
    // We don't use the standard android.media.MediaController, but a custom one.
    // See the class itself for details.
    private MediaController mMediaController;
    private RemotingSessionController mSessionController;
    private MediaRouteButton mMediaRouteButton;

    /**
     * Handle actions from on-screen media controls.
     */
    private MediaController.Delegate mControllerDelegate = new MediaController.Delegate() {
        @Override
        public void play() {
            // TODO(zqzhang): Implement via RemoteMediaClient.
        }

        @Override
        public void pause() {
            // TODO(zqzhang): Implement via RemoteMediaClient.
        }

        @Override
        public long getDuration() {
            // TODO(zqzhang): Implement via RemoteMediaClient.
            return 0;
        }

        @Override
        public long getPosition() {
            // TODO(zqzhang): Implement via RemoteMediaClient.
            return 0;
        }

        @Override
        public void seekTo(long pos) {
            // TODO(zqzhang): Implement via RemoteMediaClient.
        }

        @Override
        public boolean isPlaying() {
            // TODO(zqzhang): Implement via RemoteMediaClient.
            return false;
        }

        @Override
        public long getActionFlags() {
            // TODO(zqzhang): Implement via RemoteMediaClient.
            return 0;
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mSessionController = RemotingSessionController.getInstance();

        MediaNotificationUma.recordClickSource(getIntent());

        if (mSessionController == null || !mSessionController.isConnected()) {
            finish();
            return;
        }

        mSessionController.addCallback(this);

        // Make the activity full screen.
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);

        // requestWindowFeature must be called before adding content.
        setContentView(R.layout.expanded_cast_controller);
        // mHandler = new Handler();

        ViewGroup rootView = (ViewGroup) findViewById(android.R.id.content);
        rootView.setBackgroundColor(Color.BLACK);

        // mMediaRouteController.addUiListener(this);

        // Create and initialize the media control UI.
        mMediaController = (MediaController) findViewById(R.id.cast_media_controller);
        mMediaController.setDelegate(mControllerDelegate);

        View castButtonView = getLayoutInflater().inflate(
                R.layout.caf_controller_media_route_button, rootView, false);
        if (castButtonView instanceof MediaRouteButton) {
            mMediaRouteButton = (MediaRouteButton) castButtonView;
            rootView.addView(mMediaRouteButton);
            mMediaRouteButton.bringToFront();
            mMediaRouteButton.setRouteSelector(mSessionController.getSource().buildRouteSelector());
        }

        mMediaController.refresh();

        // TODO(zqzhang): update UI.
    }

    @Override
    protected void onResume() {
        super.onResume();

        if (mSessionController == null || !mSessionController.isConnected()) {
            finish();
            return;
        }
    }

    @Override
    public void onSessionStarted() {}

    @Override
    public void onSessionEnded() {
        finish();
    }
}

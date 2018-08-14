// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.caf;

import android.support.v7.media.MediaRouter;

import com.google.android.gms.cast.CastDevice;
import com.google.android.gms.cast.framework.CastSession;

import org.chromium.chrome.browser.media.router.CastSessionUtil;
import org.chromium.chrome.browser.media.router.MediaSink;
import org.chromium.chrome.browser.media.router.MediaSource;

import java.util.ArrayList;
import java.util.List;

/**
 * A wrapper for {@link CastSession}, extending its functionality for Chrome MediaRouter.
 *
 * Has the same lifecycle with CastSession.
 */
public class CastSessionController {
    private static final String TAG = "CastSessionController";

    private final CastSession mCastSession;
    private final CafBaseMediaRouteProvider mProvider;
    private final MediaSink mSink;
    private final MediaSource mSource;

    public CastSessionController(CastSession castSession, CafBaseMediaRouteProvider provider,
            MediaSink sink, MediaSource source) {
        mCastSession = castSession;
        mProvider = provider;
        mSink = sink;
        mSource = source;
    }

    public MediaSource getSource() {
        return mSource;
    }

    public MediaSink getSink() {
        return mSink;
    }

    public CastSession getSession() {
        return mCastSession;
    }

    public void endSession() {
        MediaRouter mediaRouter = mProvider.getAndroidMediaRouter();
        mediaRouter.selectRoute(mediaRouter.getDefaultRoute());
    }

    public List<String> getNamespaces() {
        // Not implemented.
        return new ArrayList<>();
    }

    public List<String> getCapabilities() {
        List<String> capabilities = new ArrayList<>();
        if (mCastSession == null || !mCastSession.isConnected()) return capabilities;
        CastDevice device = mCastSession.getCastDevice();
        if (device.hasCapability(CastDevice.CAPABILITY_AUDIO_IN)) {
            capabilities.add("audio_in");
        }
        if (device.hasCapability(CastDevice.CAPABILITY_AUDIO_OUT)) {
            capabilities.add("audio_out");
        }
        if (device.hasCapability(CastDevice.CAPABILITY_VIDEO_IN)) {
            capabilities.add("video_in");
        }
        if (device.hasCapability(CastDevice.CAPABILITY_VIDEO_OUT)) {
            capabilities.add("video_out");
        }
        return capabilities;
    }

    public void onSessionStarted() {
        // Not implemented.
    }

    public boolean isConnected() {
        return mCastSession != null && mCastSession.isConnected();
    }

    public void updateRemoteMediaClient(String message) {
        if (!isConnected()) return;

        mCastSession.getRemoteMediaClient().onMessageReceived(
                mCastSession.getCastDevice(), CastSessionUtil.MEDIA_NAMESPACE, message);
    }
}

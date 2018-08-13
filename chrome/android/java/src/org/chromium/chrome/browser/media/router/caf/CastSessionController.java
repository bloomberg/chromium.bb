// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.caf;

import com.google.android.gms.cast.framework.CastSession;

import org.chromium.chrome.browser.media.router.MediaSink;
import org.chromium.chrome.browser.media.router.MediaSource;

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
        CastSession currentCastSession =
                CastUtils.getCastContext().getSessionManager().getCurrentCastSession();
        if (currentCastSession == mCastSession) {
            CastUtils.getCastContext().getSessionManager().endCurrentSession(true);
        }
    }

    public void onSessionStarted() {
        // Not implemented.
    }

    public void notifyReceiverAction(
            String routeId, MediaSink sink, String clientId, String action) {
        // Not implemented.
    }
}

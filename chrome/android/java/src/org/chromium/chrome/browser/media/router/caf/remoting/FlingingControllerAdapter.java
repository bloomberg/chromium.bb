// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.caf.remoting;

import com.google.android.gms.cast.framework.media.RemoteMediaClient;

import org.chromium.chrome.browser.media.router.FlingingController;
import org.chromium.chrome.browser.media.router.MediaController;
import org.chromium.chrome.browser.media.router.MediaStatusObserver;

/** Adapter class for bridging {@link RemoteMediaClient} and {@link FlingController}. */
public class FlingingControllerAdapter implements FlingingController {
    FlingingControllerAdapter(RemotingSessionController sessionController) {}

    @Override
    public MediaController getMediaController() {
        // Not implemented.
        return null;
    }

    @Override
    public void setMediaStatusObserver(MediaStatusObserver observer) {
        // Not implemented.
    }

    @Override
    public void clearMediaStatusObserver() {
        // Not implemented.
    }

    @Override
    public long getApproximateCurrentTime() {
        // Not implemented.
        return 0;
    }
}

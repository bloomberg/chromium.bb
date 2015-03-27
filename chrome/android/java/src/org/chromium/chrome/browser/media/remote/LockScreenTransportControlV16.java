// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.remote;

import android.annotation.TargetApi;
import android.content.Context;
import android.os.Build;
import android.support.v7.media.MediaRouter;

/**
 * An implementation of {@link LockScreenTransportControl} targeting platforms with an API greater
 * than 15. Extends {@link LockScreenTransportControlV14}, adding support for remote volume control.
 */
@TargetApi(Build.VERSION_CODES.JELLY_BEAN)
class LockScreenTransportControlV16 extends LockScreenTransportControlV14 {

    private final MediaRouter mMediaRouter;

    LockScreenTransportControlV16(Context context) {
        super(context);
        mMediaRouter = MediaRouter.getInstance(context);
    }

    @Override
    protected void register() {
        super.register();
        mMediaRouter.addRemoteControlClient(getRemoteControlClient());
    }

    @Override
    protected void unregister() {
        mMediaRouter.removeRemoteControlClient(getRemoteControlClient());
        super.unregister();
    }
}

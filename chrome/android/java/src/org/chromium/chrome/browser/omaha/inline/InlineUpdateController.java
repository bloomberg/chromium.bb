// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.inline;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.support.annotation.Nullable;

import org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState;

/**
 * Helper class for gluing interactions with the Play store's AppUpdateManager with Chrome.  This
 * involves hooking up to Play as a listener for install state changes, should only happen if we are
 * in the foreground.
 */
public class InlineUpdateController {
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private final Runnable mCallback;

    private boolean mEnabled;
    private @Nullable @UpdateState Integer mUpdateState;

    /**
     * Builds an instance of {@link InlineUpdateController}.
     * @param callback The {@link Runnable} to notify when an inline update state change occurs.
     */
    public InlineUpdateController(Runnable callback) {
        mCallback = callback;

        setEnabled(true);
    }

    public void setEnabled(boolean enabled) {
        if (mEnabled == enabled) return;
        mEnabled = enabled;

        if (mEnabled) {
            mUpdateState = UpdateState.NONE;
            mHandler.post(mCallback);
        }
    }

    /**
     * @return The current state of the inline update process.  May be {@code null} if the state
     * hasn't been determined yet.
     */
    public @Nullable @UpdateState Integer getStatus() {
        return mUpdateState;
    }

    /**
     * Starts the update, if possible.  This will send an {@link Intent} out to play, which may
     * cause Chrome to move to the background.
     * @param activity The {@link Activity} to use to interact with Play.
     */
    public void startUpdate(Activity activity) {}

    /**
     * Completes the Play installation process, if possible.  This may cause Chrome to restart.
     */
    public void completeUpdate() {}
}
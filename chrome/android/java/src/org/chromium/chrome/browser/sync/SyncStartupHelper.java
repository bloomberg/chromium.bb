// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import org.chromium.base.ThreadUtils;

/**
 * Starts up the sync backend and notifies the caller once the backend finishes
 * initializing or if the backend fails to start up.
 */
public class SyncStartupHelper implements ProfileSyncService.SyncStateChangedListener {
    private static final String TAG = "SyncStartupHelper";
    private final ProfileSyncService mProfileSyncService;
    private final Delegate mDelegate;
    private boolean mDestroyed;

    /**
     * Callback interface for SyncStartupHelper.
     */
    public interface Delegate {
        // Invoked once the sync backend has successfully started up.
        public void startupComplete();

        // Invoked if the sync backend failed to start up.
        public void startupFailed();
    }

    public SyncStartupHelper(ProfileSyncService profileSyncService, Delegate delegate) {
        ThreadUtils.assertOnUiThread();
        mProfileSyncService = profileSyncService;
        mDelegate = delegate;
        assert mDelegate != null;
    }

    public void startSync() {
        ThreadUtils.assertOnUiThread();
        assert !mDestroyed;
        if (mProfileSyncService.isSyncInitialized()) {
            // Sync is already initialized - notify the delegate.
            mDelegate.startupComplete();
        } else {
            // Tell the sync engine to start up.
            mProfileSyncService.syncSignIn();
            mProfileSyncService.addSyncStateChangedListener(SyncStartupHelper.this);
            syncStateChanged();
        }
    }

    public void destroy() {
        ThreadUtils.assertOnUiThread();
        mProfileSyncService.removeSyncStateChangedListener(this);
        mDestroyed = true;
    }

    @Override
    public void syncStateChanged() {
        if (mProfileSyncService.isSyncInitialized()) {
            mDelegate.startupComplete();
        } else if ((mProfileSyncService.getAuthError() != GoogleServiceAuthError.State.NONE)
                || mProfileSyncService.hasUnrecoverableError()) {
            mDelegate.startupFailed();
        }
    }

}

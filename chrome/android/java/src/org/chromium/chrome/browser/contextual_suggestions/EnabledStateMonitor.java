// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.ProfileSyncService.SyncStateChangedListener;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.UploadState;

/**
 * A monitor that is responsible for detecting changes to conditions required for contextual
 * suggestions to be enabled. Alerts its {@link Observer} when state changes.
 */
public class EnabledStateMonitor implements SyncStateChangedListener {
    /** An observer to be notified of enabled state changes. **/
    interface Observer {
        void onEnabledStateChanged(boolean enabled);
    }

    private final Observer mObserver;
    private boolean mEnabled;

    /**
     * Construct a new {@link EnabledStateMonitor}.
     * @param observer The {@link Observer} to be notified of changes to enabled state.
     */
    EnabledStateMonitor(Observer observer) {
        mObserver = observer;
        ProfileSyncService.get().addSyncStateChangedListener(this);
        updateEnabledState();
    }

    void destroy() {
        ProfileSyncService.get().removeSyncStateChangedListener(this);
    }

    @Override
    public void syncStateChanged() {
        updateEnabledState();
    }

    /**
     * Updates whether contextual suggestions are enabled. Notifies the observer if the
     * enabled state has changed.
     */
    private void updateEnabledState() {
        boolean previousState = mEnabled;

        ProfileSyncService service = ProfileSyncService.get();
        mEnabled = service.getUploadToGoogleState(ModelType.HISTORY_DELETE_DIRECTIVES)
                == UploadState.ACTIVE;

        // TODO(twellington): Add other run-time checks, e.g. enterprise policy, opt-out state.

        if (mEnabled != previousState) mObserver.onEnabledStateChanged(mEnabled);
    }
}

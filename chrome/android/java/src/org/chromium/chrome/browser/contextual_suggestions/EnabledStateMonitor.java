// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;
import org.chromium.chrome.browser.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.ProfileSyncService.SyncStateChangedListener;
import org.chromium.chrome.browser.util.AccessibilityUtil;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.UploadState;

/**
 * A monitor that is responsible for detecting changes to conditions required for contextual
 * suggestions to be enabled. Alerts its {@link Observer} when state changes.
 */
public class EnabledStateMonitor
        implements SyncStateChangedListener, SignInStateObserver, TemplateUrlServiceObserver {
    /** An observer to be notified of enabled state changes. **/
    interface Observer {
        void onEnabledStateChanged(boolean enabled);
    }

    @VisibleForTesting
    protected Observer mObserver;
    private boolean mEnabled;

    /**
     * Construct a new {@link EnabledStateMonitor}.
     * @param observer The {@link Observer} to be notified of changes to enabled state.
     */
    EnabledStateMonitor(Observer observer) {
        mObserver = observer;
        init();
    }

    /**
     * Initializes the EnabledStateMonitor. Intended to encapsulate creating connections to native
     * code, so that this can be easily stubbed out during tests. This method should only be
     * overridden for testing.
     */
    @VisibleForTesting
    protected void init() {
        ProfileSyncService.get().addSyncStateChangedListener(this);
        SigninManager.get().addSignInStateObserver(this);
        TemplateUrlService.getInstance().addObserver(this);
        updateEnabledState();
    }

    /** Destroys the EnabledStateMonitor. */
    void destroy() {
        ProfileSyncService.get().removeSyncStateChangedListener(this);
        SigninManager.get().removeSignInStateObserver(this);
        TemplateUrlService.getInstance().removeObserver(this);
    }

    /** Called when accessibility mode changes. */
    void onAccessibilityModeChanged() {
        updateEnabledState();
    }

    @Override
    public void syncStateChanged() {
        updateEnabledState();
    }

    @Override
    public void onSignedIn() {
        updateEnabledState();
    }

    @Override
    public void onSignedOut() {
        updateEnabledState();
    }

    @Override
    public void onTemplateURLServiceChanged() {
        updateEnabledState();
    }

    /**
     * Updates whether contextual suggestions are enabled. Notifies the observer if the
     * enabled state has changed.
     */
    private void updateEnabledState() {
        boolean previousState = mEnabled;

        ProfileSyncService service = ProfileSyncService.get();

        boolean isUploadToGoogleActive =
                service.getUploadToGoogleState(ModelType.HISTORY_DELETE_DIRECTIVES)
                == UploadState.ACTIVE;
        boolean isGoogleDSE = TemplateUrlService.getInstance().isDefaultSearchEngineGoogle();
        boolean isAccessibilityEnabled = AccessibilityUtil.isAccessibilityEnabled();

        mEnabled = isUploadToGoogleActive && isGoogleDSE && !isAccessibilityEnabled
                && !ContextualSuggestionsBridge.isEnterprisePolicyManaged();

        // TODO(twellington): Add run-time check for opt-out state.

        if (mEnabled != previousState) mObserver.onEnabledStateChanged(mEnabled);
    }
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import android.content.Context;
import android.os.Bundle;

/**
 * Base class for Policy providers.
 */
public abstract class PolicyProvider {
    private PolicyManager mPolicyManager;
    protected final Context mContext;
    private int mSource = -1;

    protected PolicyProvider(Context context) {
        mContext = context.getApplicationContext();
    }

    protected void notifySettingsAvailable(Bundle settings) {
        mPolicyManager.onSettingsAvailable(mSource, settings);
    }

    protected void terminateIncognitoSession() {
        mPolicyManager.terminateIncognitoSession();
    }

    /**
     * Called to request a refreshed set of policies.
     * This method must handle notifying the PolicyManager whenever applicable.
     */
    public abstract void refresh();

    /**
     * Register the PolicyProvider for receiving policy changes.
     */
    protected void startListeningForPolicyChanges() {
    }

    /**
     * Called by the {@link PolicyManager} to correctly hook it with the Policy system.
     * @param policyManager reference to the PolicyManager to be used like a delegate.
     * @param source tags the PolicyProvider with a source.
     */
    final void setManagerAndSource(PolicyManager policyManager, int source) {
        assert mSource < 0;
        assert source >= 0;
        mSource = source;
        assert mPolicyManager == null;
        mPolicyManager = policyManager;
        startListeningForPolicyChanges();
    }

    /** Called when the provider is unregistered */
    public void destroy() {
    }
}

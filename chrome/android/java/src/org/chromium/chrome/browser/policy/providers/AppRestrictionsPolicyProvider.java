// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy.providers;

import android.content.Context;
import android.os.Bundle;

import org.chromium.policy.AppRestrictionsProvider;
import org.chromium.policy.PolicyProvider;

/**
 * Policy provider for Android's App Restriction Schema.
 */
public final class AppRestrictionsPolicyProvider
        extends PolicyProvider implements AppRestrictionsProvider.Delegate {
    private final AppRestrictionsProvider mAppRestrictionsProvider;

    /**
     * Register to receive the intent for App Restrictions.
     */
    public AppRestrictionsPolicyProvider(Context context) {
        super(context);
        mAppRestrictionsProvider = new AppRestrictionsProvider(context, this);
    }

    @Override
    protected void startListeningForPolicyChanges() {
        mAppRestrictionsProvider.startListening();
    }

    @Override
    public void refresh() {
        mAppRestrictionsProvider.refreshRestrictions();
    }

    @Override
    public void destroy() {
        mAppRestrictionsProvider.stopListening();
        super.destroy();
    }

    @Override
    public void notifyNewAppRestrictionsAvailable(Bundle newAppRestrictions) {
        notifySettingsAvailable(newAppRestrictions);
    }
}

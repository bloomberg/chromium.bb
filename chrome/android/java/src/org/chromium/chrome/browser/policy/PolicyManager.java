// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import android.os.Bundle;

import org.chromium.base.CalledByNative;
import org.chromium.base.ThreadUtils;
import org.chromium.policy.PolicyConverter;

import java.util.ArrayList;
import java.util.List;

/**
 * Reads enterprise policies from one or more policy providers
 * and plumbs them through to the policy subsystem.
 */
public class PolicyManager {
    private long mNativePolicyManager;

    private PolicyConverter mPolicyConverter;
    private final List<PolicyProvider> mPolicyProviders = new ArrayList<>();
    private final List<Bundle> mCachedPolicies = new ArrayList<>();
    private final List<PolicyChangeListener> mPolicyChangeListeners = new ArrayList<>();

    public void initializeNative() {
        ThreadUtils.assertOnUiThread();
        mNativePolicyManager = nativeInit();
        mPolicyConverter = nativeCreatePolicyConverter(mNativePolicyManager);
    }

    /**
     * PolicyProviders are assigned a unique precedence based on their order of registration.
     * Later Registration -> Higher Precedence.
     * This precedence is also used as a 'source' tag for disambiguating updates.
     */
    public void registerProvider(PolicyProvider provider) {
        mPolicyProviders.add(provider);
        mCachedPolicies.add(null);
        provider.setManagerAndSource(this, mPolicyProviders.size() - 1);
        provider.refresh();
    }

    public void destroy() {
        // All the activities registered should have been destroyed by then.
        assert mPolicyChangeListeners.isEmpty();

        for (PolicyProvider provider : mPolicyProviders) {
            provider.destroy();
        }
        mPolicyProviders.clear();

        nativeDestroy(mNativePolicyManager);
        mNativePolicyManager = 0;
        mPolicyConverter = null;
    }

    void onSettingsAvailable(int source, Bundle newSettings) {
        mCachedPolicies.set(source, newSettings);
        // Check if we have policies from all the providers before applying them.
        for (Bundle settings : mCachedPolicies) {
            if (settings == null) return;
        }
        for (Bundle settings : mCachedPolicies) {
            for (String key : settings.keySet()) {
                mPolicyConverter.setPolicy(key, settings.get(key));
            }
        }
        nativeFlushPolicies(mNativePolicyManager);
    }

    void terminateIncognitoSession() {
        for (PolicyChangeListener listener : mPolicyChangeListeners) {
            listener.terminateIncognitoSession();
        }
    }

    public void addPolicyChangeListener(PolicyChangeListener listener) {
        mPolicyChangeListeners.add(listener);
    }

    public void removePolicyChangeListener(PolicyChangeListener listener) {
        assert mPolicyChangeListeners.contains(listener);
        mPolicyChangeListeners.remove(listener);
    }

    @CalledByNative
    private void refreshPolicies() {
        assert mPolicyProviders.size() == mCachedPolicies.size();
        for (int i = 0; i < mCachedPolicies.size(); ++i) {
            mCachedPolicies.set(i, null);
        }
        for (PolicyProvider provider : mPolicyProviders) {
            provider.refresh();
        }
    }

    /**
     * Interface to handle actions related with policy changes.
     */
    public interface PolicyChangeListener {
        /**
         * Call to notify the listener that incognito browsing is unavailable due to policy.
         */
        void terminateIncognitoSession();
    }

    private native long nativeInit();
    private native PolicyConverter nativeCreatePolicyConverter(long nativePolicyManager);
    private native void nativeDestroy(long nativePolicyManager);
    private native void nativeFlushPolicies(long nativePolicyManager);
}

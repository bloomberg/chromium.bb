// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core;

import android.preference.PreferenceFragment;

import org.chromium.blimp.core.settings.AboutBlimpPreferences;
import org.chromium.blimp.core.settings.BlimpPreferencesDelegate;
import org.chromium.blimp_public.BlimpClientContext;
import org.chromium.blimp_public.BlimpClientContextDelegate;
import org.chromium.blimp_public.contents.BlimpContents;
import org.chromium.ui.base.WindowAndroid;

import java.util.HashMap;
import java.util.Map;

/**
 * Mock {@link BlimpClientContext}.
 * This test support class only tests Java layer and is not backed by any native code.
 */
public class MockBlimpClientContext implements BlimpClientContext, BlimpPreferencesDelegate {
    public MockBlimpClientContext() {}

    private MockBlimpClientContextDelegate mDelegate = new MockBlimpClientContextDelegate();

    private boolean mEnabled = false;
    private int mConnectCalled = 0;

    public void reset() {
        mConnectCalled = 0;
    }

    public void setBlimpEnabled(boolean enabled) {
        mEnabled = enabled;
    }

    public int connectCalledCount() {
        return mConnectCalled;
    }

    @Override
    public BlimpContents createBlimpContents(WindowAndroid window) {
        return null;
    }

    @Override
    public boolean isBlimpSupported() {
        return true;
    }

    @Override
    public void attachBlimpPreferences(PreferenceFragment fragment) {}

    @Override
    public void setDelegate(BlimpClientContextDelegate delegate) {}

    @Override
    public boolean isBlimpEnabled() {
        return mEnabled;
    }

    @Override
    public void connect() {
        mConnectCalled++;
    }

    @Override
    public BlimpClientContextDelegate getDelegate() {
        return mDelegate;
    }

    @Override
    public void initSettingsPage(AboutBlimpPreferences preferences) {}

    @Override
    public Map<String, String> getFeedbackMap() {
        return new HashMap<>();
    }
}

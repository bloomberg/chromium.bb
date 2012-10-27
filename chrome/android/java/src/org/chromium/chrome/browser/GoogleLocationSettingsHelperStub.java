// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;

import org.chromium.base.CalledByNative;

/**
 * Stub implementation in Chrome for GoogleLocationSettingsHelper.
 */

public class GoogleLocationSettingsHelperStub implements GoogleLocationSettingsHelper {

    private Context mApplicationContext;

    public GoogleLocationSettingsHelperStub(Context context) {
        mApplicationContext = context.getApplicationContext();
    }

    @Override
    public String getInfoBarAcceptLabel() {
        return "Allow";
    }

    @Override
    public boolean isMasterLocationSettingEnabled() {
        return true;
    }

    @Override
    public boolean isGoogleAppsLocationSettingEnabled() {
        return true;
    }

    @Override
    public void startGoogleLocationSettingsActivity() {
    }

    @CalledByNative
    public static GoogleLocationSettingsHelper createInstance(Context context) {
        return new GoogleLocationSettingsHelperStub(context);
    }

}

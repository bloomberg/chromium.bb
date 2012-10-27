// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.base.CalledByNative;

/**
 * Helper interface to read the Google Apps location setting to update the infobar button label.
 * Also, starts the Android Google Apps location settings activity upon request.
 */
public interface GoogleLocationSettingsHelper {

    @CalledByNative
    public String getInfoBarAcceptLabel();

    @CalledByNative
    public boolean isMasterLocationSettingEnabled();

    @CalledByNative
    public boolean isGoogleAppsLocationSettingEnabled();

    @CalledByNative
    public void startGoogleLocationSettingsActivity();
}

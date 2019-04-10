// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.locationcustomizations;

import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;

/**
 * Does customizations specified by users locations.
 */
public class LocationCustomizations {
    /**
     * Initialize location customizations.
     */
    public static void initialize() {
        if (LocaleManager.getInstance().isSpecialUser()) {
            PrefServiceBridge.getInstance().setBoolean(Pref.NTP_ARTICLES_SECTION_ENABLED, false);
        }
    }
}

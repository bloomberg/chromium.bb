// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;
import org.chromium.components.permissions.nfc.NfcSystemLevelSetting;

/**
 * A class for dealing with the NFC category.
 */
public class NfcCategory extends SiteSettingsCategory {
    public NfcCategory(BrowserContextHandle browserContextHandle) {
        // As NFC is not a per-app permission, passing an empty string means the NFC permission is
        // always enabled for Chrome.
        super(browserContextHandle, Type.NFC, "" /* androidPermission*/);
    }

    @Override
    protected boolean supportedGlobally() {
        return NfcSystemLevelSetting.isNfcAccessPossible();
    }

    @Override
    protected String getMessageIfNotSupported(Activity activity) {
        return activity.getResources().getString(R.string.android_nfc_unsupported);
    }

    @Override
    protected boolean enabledGlobally() {
        return NfcSystemLevelSetting.isNfcSystemLevelSettingEnabled();
    }

    @Override
    protected Intent getIntentToEnableOsGlobalPermission(Context context) {
        return NfcSystemLevelSetting.getNfcSystemLevelSettingIntent();
    }

    @Override
    protected String getMessageForEnablingOsGlobalPermission(Activity activity) {
        return activity.getResources().getString(R.string.android_nfc_off_globally);
    }
}

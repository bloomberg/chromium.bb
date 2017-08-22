// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.view.WindowManager;

import com.google.vr.ndk.base.AndroidCompat;

import org.chromium.chrome.browser.customtabs.SeparateTaskCustomTabActivity;

/**
 * A subclass of SeparateTaskCustomTabActivity created when starting Chrome in VR mode.
 *
 * The main purpose of this activity is to add flexibility to the way Chrome is started when the
 * user's phone is already in their VR headset (e.g, we want to hide the System UI).
 */
public class SeparateTaskCustomTabVrActivity extends SeparateTaskCustomTabActivity {
    @Override
    public void preInflationStartup() {
        if (VrIntentUtils.getHandlerInstance().isTrustedDaydreamIntent(getIntent())) {
            AndroidCompat.setVrModeEnabled(this, true);

            // Set VR specific window mode.
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            getWindow().getDecorView().setSystemUiVisibility(VrShellDelegate.VR_SYSTEM_UI_FLAGS);
        }
        super.preInflationStartup();
    }
}

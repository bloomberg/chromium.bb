// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;

import org.chromium.chrome.browser.util.IntentUtils;

/**
 * Handles the DOFF flow when Chrome is launched in VR mode and the FRE is not complete. This is
 * needed because VrCore doesn't allow calling DOFF from a non-active VR activity (b/63435686), so
 * we can't do it from {@link FirstRunActivity}. The android:enableVrMode attribute in the manifest
 * makes this a VR activity allowing us to call DOFF before triggering the standard 2D FRE flow.
 * This should be removed when b/63435686 is fixed.
 */
public class VrFirstRunActivity extends Activity {
    private static final long SHOW_DOFF_TIMEOUT_MS = 500;

    private VrDaydreamApi mApi;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        assert VrIntentUtils.isVrIntent(getIntent());
        VrClassesWrapper wrapper = VrShellDelegate.createVrClassesWrapper();
        if (wrapper == null) {
            showFre();
            return;
        }

        // VrCore version M21 allows a transitioning VR activity to call DOFF and requires that
        // we set VR mode programmatically. Up until then, this hack of having a pure VR activity
        // works, but there's still a race every now and then that causes Chrome to crash and
        // that's why we have a timeout below before we call DOFF. Redundantly setting VR mode
        // here ensures that this never happens for users running the latest version of VrCore.
        wrapper.setVrModeEnabled(this, true);
        mApi = wrapper.createVrDaydreamApi(this);
        if (!mApi.isDaydreamCurrentViewer()) {
            showFre();
            return;
        }
        // Show DOFF with a timeout so that this activity has enough time to be the active VR app.
        new Handler().postDelayed(new Runnable() {
            @Override
            public void run() {
                mApi.exitFromVr(VrShellDelegate.EXIT_VR_RESULT, new Intent());
            }
        }, SHOW_DOFF_TIMEOUT_MS);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent intent) {
        if (requestCode != VrShellDelegate.EXIT_VR_RESULT) return;
        if (resultCode == Activity.RESULT_OK) {
            showFre();
            return;
        }
        finish();
        mApi.launchVrHomescreen();
    }

    private void showFre() {
        // Start the actual 2D FRE if the user successfully exited VR.
        Intent freIntent = (Intent) IntentUtils.safeGetParcelableExtra(
                getIntent(), VrIntentUtils.VR_FRE_INTENT_EXTRA);
        IntentUtils.safeStartActivity(this, freIntent);
        finish();
    }
}

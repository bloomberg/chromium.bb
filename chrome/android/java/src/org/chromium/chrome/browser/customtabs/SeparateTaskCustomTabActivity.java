// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;
import android.os.Build;

import org.chromium.base.ApiCompatibilityUtils;

/**
 * Simple wrapper around the CustomTabActivity to be used when launching each CustomTab in a
 * separate task.
 */
// TODO(tedchoc): Add support for updating the Android task info on L+.
public class SeparateTaskCustomTabActivity extends CustomTabActivity {

    private boolean mDidFinishForReparenting;

    @Override
    public void preInflationStartup() {
        Intent intent = getIntent();
        if (intent != null
                && (intent.getFlags() & Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS) != 0) {
            // TODO(tedchoc): Less hacky town.
            // This is removed in onDestroy and complains if it is not added, so we just add it
            // as a temporary work around.
            getChromeApplication().addPolicyChangeListener(this);
            finish();
            return;
        }
        super.preInflationStartup();
    }

    @Override
    public void onStop() {
        super.onStop();

        mDidFinishForReparenting = false;
    }

    @Override
    public void finishAndClose() {
        if (mDidFinishForReparenting) return;

        mDidFinishForReparenting = true;
        if (getCallingActivity() != null) {
            finish();
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            ApiCompatibilityUtils.finishAndRemoveTask(this);
        } else {
            // TODO(tedchoc): This does not work reliably :-/.  Need to find a solution for the X
            //                button and the Android back.  Seems to only somewhat work for the
            //                open in Chrome case.
            Intent intent = new Intent(getIntent());
            intent.addFlags(Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS);
            startActivity(intent);
            overridePendingTransition(0, 0);
        }
    }
}

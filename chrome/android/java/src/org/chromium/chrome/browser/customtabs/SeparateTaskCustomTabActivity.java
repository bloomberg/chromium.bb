// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;
import android.os.Build;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabTaskDescriptionHelper;

/**
 * Simple wrapper around the CustomTabActivity to be used when launching each CustomTab in a
 * separate task.
 */
public class SeparateTaskCustomTabActivity extends CustomTabActivity {

    private boolean mDidFinishForReparenting;
    private ActivityTabTaskDescriptionHelper mTaskDescriptionHelper;

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
    public void finishNativeInitialization() {
        super.finishNativeInitialization();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            mTaskDescriptionHelper = new ActivityTabTaskDescriptionHelper(this,
                    ApiCompatibilityUtils.getColor(getResources(), R.color.default_primary_color));
        }
    }

    @Override
    public void onStop() {
        super.onStop();

        mDidFinishForReparenting = false;
    }

    @Override
    protected void onDestroyInternal() {
        super.onDestroyInternal();

        if (mTaskDescriptionHelper != null) mTaskDescriptionHelper.destroy();
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

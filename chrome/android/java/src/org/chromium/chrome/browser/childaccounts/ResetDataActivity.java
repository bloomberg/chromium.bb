// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.childaccounts;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.os.Build;
import android.os.Bundle;

import org.chromium.chrome.browser.externalauth.ExternalAuthUtils;

/**
 * An activity that allows whitelisted applications to reset all data in Chrome, as part of
 * child account management.
 */
public class ResetDataActivity extends Activity {
    /**
     * The calling activity is not authorized. This activity is only available to Google-signed
     * applications.
     */
    private static final int RESULT_ERROR_UNAUTHORIZED = Activity.RESULT_FIRST_USER;

    /**
     * Resetting data is not supported.
     */
    private static final int RESULT_ERROR_NOT_SUPPORTED = Activity.RESULT_FIRST_USER + 1;

    /**
     * There was an error resetting data.
     */
    private static final int RESULT_ERROR_COULD_NOT_RESET_DATA = Activity.RESULT_FIRST_USER + 2;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (!authenticateSender()) {
            returnResult(RESULT_ERROR_UNAUTHORIZED);
            return;
        }

        // If resetting data is not supported, immediately return an error.
        if (!supportsResetData()) {
            returnResult(RESULT_ERROR_NOT_SUPPORTED);
            return;
        }

        boolean success = resetData();

        // We should only land here if resetting data was not successful, as otherwise the process
        // will be killed.
        assert !success;
        returnResult(RESULT_ERROR_COULD_NOT_RESET_DATA);
    }

    private boolean authenticateSender() {
        return ExternalAuthUtils.getInstance().isGoogleSigned(this, getCallingPackage());
    }

    private boolean supportsResetData() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT;
    }

    @TargetApi(Build.VERSION_CODES.KITKAT)
    private boolean resetData() {
        assert supportsResetData();
        ActivityManager am = (ActivityManager) getSystemService(Context.ACTIVITY_SERVICE);
        return am.clearApplicationUserData();
    }

    private void returnResult(int resultCode) {
        setResult(resultCode);
        finish();
    }
}

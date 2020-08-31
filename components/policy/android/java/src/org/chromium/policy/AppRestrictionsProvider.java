// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.policy;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.os.UserManager;

import org.chromium.base.metrics.RecordHistogram;

/**
 * Concrete app restriction provider, that uses the default android mechanism to retrieve the
 * restrictions.
 */
public class AppRestrictionsProvider extends AbstractAppRestrictionsProvider {
    private final UserManager mUserManager;

    public AppRestrictionsProvider(Context context) {
        super(context);

        // getApplicationRestrictions method of UserManager was introduced in JELLY_BEAN_MR2.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
            mUserManager = (UserManager) context.getSystemService(Context.USER_SERVICE);
        } else {
            mUserManager = null;
        }
    }

    @Override
    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    protected Bundle getApplicationRestrictions(String packageName) {
        if (mUserManager == null) return new Bundle();
        try {
            long startTime = SystemClock.elapsedRealtime();
            Bundle bundle = mUserManager.getApplicationRestrictions(packageName);
            long endTime = SystemClock.elapsedRealtime();
            long duration = endTime - startTime;
            RecordHistogram.recordTimesHistogram("Enterprise.AppRestrictionLoadTime2", duration);
            if (bundle.isEmpty()) {
                RecordHistogram.recordTimesHistogram(
                        "Enterprise.AppRestrictionLoadTime2.EmptyBundle", duration);
            } else {
                RecordHistogram.recordTimesHistogram(
                        "Enterprise.AppRestrictionLoadTime2.NonEmptyBundle", duration);
            }
            return bundle;
        } catch (SecurityException e) {
            // Android bug may throw SecurityException. See crbug.com/886814.
            return new Bundle();
        }
    }

    @Override
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    protected String getRestrictionChangeIntentAction() {
        // Intent.ACTION_APPLICATION_RESTRICTIONS_CHANGED was introduced in LOLLIPOP.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return null;
        return Intent.ACTION_APPLICATION_RESTRICTIONS_CHANGED;
    }
}

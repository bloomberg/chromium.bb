// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.util.Log;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.ThreadUtils;
import org.chromium.content.common.ScreenOrientationValues;

/**
 * This is the implementation of the C++ counterpart ScreenOrientationProvider.
 */
@JNINamespace("content")
class ScreenOrientationProvider {
    private static final String TAG = "ScreenOrientationProvider";

    private static int getOrientationFromWebScreenOrientations(byte orientations) {
        switch (orientations) {
            case ScreenOrientationValues.DEFAULT:
                return ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
            case ScreenOrientationValues.PORTRAIT_PRIMARY:
                return ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
            case ScreenOrientationValues.PORTRAIT_SECONDARY:
                return ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT;
            case ScreenOrientationValues.LANDSCAPE_PRIMARY:
                return ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
            case ScreenOrientationValues.LANDSCAPE_SECONDARY:
                return ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE;
            case ScreenOrientationValues.PORTRAIT:
                return ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT;
            case ScreenOrientationValues.LANDSCAPE:
                return ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE;
            case ScreenOrientationValues.ANY:
                return ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR;
            default:
                Log.w(TAG, "Trying to lock to unsupported orientation!");
                return ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
        }
    }

    @CalledByNative
    static void lockOrientation(byte orientations) {
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (activity == null) {
            return;
        }

        int orientation = getOrientationFromWebScreenOrientations(orientations);
        if (orientation == ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED) {
            return;
        }

        activity.setRequestedOrientation(orientation);
    }

    @CalledByNative
    static void unlockOrientation() {
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (activity == null) {
            return;
        }

        int defaultOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;

        try {
            ActivityInfo info = activity.getPackageManager().getActivityInfo(
                    activity.getComponentName(), PackageManager.GET_META_DATA);
            defaultOrientation = info.screenOrientation;
        } catch (PackageManager.NameNotFoundException e) {
            // Do nothing, defaultOrientation should be SCREEN_ORIENTATION_UNSPECIFIED.
        } finally {
            activity.setRequestedOrientation(defaultOrientation);
        }
    }

    @CalledByNative
    static void startAccurateListening() {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                ScreenOrientationListener.getInstance().startAccurateListening();
            }
        });
    }

    @CalledByNative
    static void stopAccurateListening() {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                ScreenOrientationListener.getInstance().stopAccurateListening();
            }
        });
    }

    private ScreenOrientationProvider() {
    }
}

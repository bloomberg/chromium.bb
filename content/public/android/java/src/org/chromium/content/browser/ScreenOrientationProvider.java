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
import org.chromium.content_public.common.ScreenOrientationConstants;
import org.chromium.content_public.common.ScreenOrientationValues;
import org.chromium.ui.gfx.DeviceDisplayInfo;

/**
 * This is the implementation of the C++ counterpart ScreenOrientationProvider.
 */
@JNINamespace("content")
public class ScreenOrientationProvider {
    private static final String TAG = "ScreenOrientationProvider";

    private static int getOrientationFromWebScreenOrientations(byte orientation,
            Activity activity) {
        switch (orientation) {
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
            case ScreenOrientationValues.NATURAL:
                DeviceDisplayInfo displayInfo = DeviceDisplayInfo.create(activity);
                int rotation = displayInfo.getRotationDegrees();
                if (rotation == 0 || rotation == 180) {
                    if (displayInfo.getDisplayHeight() >= displayInfo.getDisplayWidth()) {
                        return ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
                    }
                    return ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
                } else {
                    if (displayInfo.getDisplayHeight() < displayInfo.getDisplayWidth()) {
                        return ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
                    }
                    return ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
                }
            default:
                Log.w(TAG, "Trying to lock to unsupported orientation!");
                return ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
        }
    }

    @CalledByNative
    static void lockOrientation(byte orientation) {
        lockOrientation(orientation, ApplicationStatus.getLastTrackedFocusedActivity());
    }

    public static void lockOrientation(byte webScreenOrientation, Activity activity) {
        if (activity == null) return;

        int orientation = getOrientationFromWebScreenOrientations(webScreenOrientation, activity);
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

        // Activities opened from a shortcut may have EXTRA_ORIENTATION set. In
        // which case, we want to use that as the default orientation.
        int orientation = activity.getIntent().getIntExtra(
                ScreenOrientationConstants.EXTRA_ORIENTATION,
                ScreenOrientationValues.DEFAULT);
        defaultOrientation = getOrientationFromWebScreenOrientations(
                (byte) orientation, activity);

        try {
            if (defaultOrientation == ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED) {
                ActivityInfo info = activity.getPackageManager().getActivityInfo(
                        activity.getComponentName(), PackageManager.GET_META_DATA);
                defaultOrientation = info.screenOrientation;
            }
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

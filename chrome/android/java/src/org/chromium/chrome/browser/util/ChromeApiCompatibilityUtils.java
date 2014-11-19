// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Build;
import android.os.PowerManager;
import android.provider.Settings;
import android.view.Window;
import android.view.WindowManager;

import org.chromium.base.ThreadUtils;

/**
 * Utility class to use new APIs that were added after ICS (API level 14).
 */
public class ChromeApiCompatibilityUtils {
    /**
     * @see android.app.Activity#finishAndRemoveTask()
     */
    public static void finishAndRemoveTask(Activity activity) {
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.LOLLIPOP) {
            activity.finishAndRemoveTask();
        } else if (Build.VERSION.SDK_INT == Build.VERSION_CODES.LOLLIPOP) {
            // crbug.com/395772 : Fallback for Activity.finishAndRemoveTask() failing.
            new FinishAndRemoveTaskWithRetry(activity).run();
        } else {
            activity.finish();
        }
    }

    private static class FinishAndRemoveTaskWithRetry implements Runnable {
        private static final long RETRY_DELAY_MS = 500;
        private static final long MAX_TRY_COUNT = 3;
        private final Activity mActivity;
        private int mTryCount;

        FinishAndRemoveTaskWithRetry(Activity activity) {
            mActivity = activity;
        }

        @Override
        public void run() {
            mActivity.finishAndRemoveTask();
            mTryCount++;
            if (!mActivity.isFinishing()) {
                if (mTryCount < MAX_TRY_COUNT) {
                    ThreadUtils.postOnUiThreadDelayed(this, RETRY_DELAY_MS);
                } else {
                    mActivity.finish();
                }
            }
        }
    }

    /**
     * @return Whether the screen of the device is interactive.
     */
    @SuppressWarnings("deprecation")
    public static boolean isInteractive(Context context) {
        PowerManager manager = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT_WATCH) {
            return manager.isInteractive();
        } else {
            return manager.isScreenOn();
        }
    }

    @SuppressWarnings("deprecation")
    public static int getActivityNewDocumentFlag() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            return Intent.FLAG_ACTIVITY_NEW_DOCUMENT;
        } else {
            return Intent.FLAG_ACTIVITY_CLEAR_WHEN_TASK_RESET;
        }
    }

    /**
     * @see android.provider.Settings.Secure#SKIP_FIRST_USE_HINTS
     */
    public static boolean shouldSkipFirstUseHints(ContentResolver contentResolver) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            return Settings.Secure.getInt(
                    contentResolver, Settings.Secure.SKIP_FIRST_USE_HINTS, 0) != 0;
        } else {
            return false;
        }
    }

    /**
     * @param activity Activity that should get the task description update.
     * @param title Title of the activity.
     * @param icon Icon of the activity.
     * @param color Color of the activity.
     */
    public static void setTaskDescription(Activity activity, String title, Bitmap icon, int color) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            ActivityManager.TaskDescription description =
                    new ActivityManager.TaskDescription(title, icon, color);
            activity.setTaskDescription(description);
        }
    }

    /**
     * @see android.view.Window#setStatusBarColor(int color).
     */
    public static void setStatusBarColor(Activity activity, int statusBarColor) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            // If both system bars are black, we can remove these from our layout,
            // removing or shrinking the SurfaceFlinger overlay required for our views.
            Window window = activity.getWindow();
            if (statusBarColor == Color.BLACK && window.getNavigationBarColor() == Color.BLACK) {
                window.clearFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
            } else {
                window.addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
            }
            window.setStatusBarColor(statusBarColor);
        }
    }
}

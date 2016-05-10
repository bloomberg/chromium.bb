// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Utilities for detecting multi-window/multi-instance support.
 *
 * Thread-safe: This class may be accessed from any thread.
 */
public class MultiWindowUtils {

    // TODO(twellington): replace this with Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT once we're building
    //                    against N.
    public static final int FLAG_ACTIVITY_LAUNCH_ADJACENT = 0x00001000;

    private static AtomicReference<MultiWindowUtils> sInstance =
            new AtomicReference<MultiWindowUtils>();

    /**
     * Returns the singleton instance of MultiWindowUtils, creating it if needed.
     */
    public static MultiWindowUtils getInstance() {
        if (sInstance.get() == null) {
            ChromeApplication application =
                    (ChromeApplication) ContextUtils.getApplicationContext();
            sInstance.compareAndSet(null, application.createMultiWindowUtils());
        }
        return sInstance.get();
    }

    /**
     * @param activity The {@link Activity} to check.
     * @return Whether or not {@code activity} is currently in Android N+ multi-window mode.
     */
    public boolean isInMultiWindowMode(Activity activity) {
        if (activity == null) return false;

        if (Build.VERSION.CODENAME.equals("N")) {
            try {
                Method isInMultiWindowModeMethod = Activity.class.getMethod("isInMultiWindowMode");
                boolean isInMultiWindowMode = (boolean) isInMultiWindowModeMethod.invoke(activity);
                return isInMultiWindowMode;
            } catch (NoSuchMethodException e) {
                // Ignore.
            } catch (IllegalAccessException e) {
                // Ignore.
            } catch (IllegalArgumentException e) {
                // Ignore.
            } catch (InvocationTargetException e) {
                // Ignore.
            }
        }

        return false;
    }

    /**
     * Returns whether the given activity currently supports opening tabs in or moving tabs to the
     * other window.
     */
    public boolean isOpenInOtherWindowSupported(Activity activity) {
        // Supported only in multi-window mode and if activity supports side-by-side instances.
        return isInMultiWindowMode(activity) && getOpenInOtherWindowActivity(activity) != null;
    }

    /**
     * Returns the activity to use when handling "open in other window" or "move to other window".
     * Returns null if the current activity doesn't support opening/moving tabs to another activity.
     */
    public Class<? extends Activity> getOpenInOtherWindowActivity(Activity current) {
        if (current instanceof ChromeTabbedActivity2) {
            return ChromeTabbedActivity.class;
        } else if (current instanceof ChromeTabbedActivity) {
            return ChromeTabbedActivity2.class;
        } else {
            return null;
        }
    }

    /**
     * @param activity The {@link Activity} to check.
     * @return Whether or not {@code activity} is currently in pre-N Samsung multi-window mode.
     */
    public boolean isLegacyMultiWindow(Activity activity) {
        // This logic is overridden in a subclass.
        return false;
    }

    /**
     * @param activity The {@link Activity} to check.
     * @return Whether or not {@code activity} should run in pre-N Samsung multi-instance mode.
     */
    public boolean shouldRunInLegacyMultiInstanceMode(ChromeLauncherActivity activity) {
        return Build.VERSION.SDK_INT > Build.VERSION_CODES.LOLLIPOP
                && TextUtils.equals(activity.getIntent().getAction(), Intent.ACTION_MAIN)
                && isLegacyMultiWindow(activity)
                && activity.isChromeBrowserActivityRunning();
    }

    /**
     * Makes |intent| able to support multi-instance in pre-N Samsung multi-window mode.
     */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public void makeLegacyMultiInstanceIntent(ChromeLauncherActivity activity, Intent intent) {
        if (isLegacyMultiWindow(activity)) {
            if (TextUtils.equals(ChromeTabbedActivity.class.getName(),
                    intent.getComponent().getClassName())) {
                intent.setClassName(activity, MultiInstanceChromeTabbedActivity.class.getName());
            }
            intent.setFlags(intent.getFlags()
                    & ~(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT));
        }
    }
}

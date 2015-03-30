// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.os.Bundle;
import android.os.Parcelable;

import java.util.ArrayList;
import java.util.List;

/**
 * Utilities dealing with extracting information from intents.
 */
public class IntentUtils {

    /**
     * Retrieves a list of components that would handle the given intent.
     * @param context The application context.
     * @param intent The intent which we are interested in.
     * @return The list of component names.
     */
    public static List<ComponentName> getIntentHandlers(Context context, Intent intent) {
        List<ResolveInfo> list = context.getPackageManager().queryIntentActivities(intent, 0);
        List<ComponentName> nameList = new ArrayList<ComponentName>();
        for (ResolveInfo r : list) {
            nameList.add(new ComponentName(r.activityInfo.packageName, r.activityInfo.name));
        }
        return nameList;
    }

    /**
     * Just like {@link Intent.getBooleanExtra()} but doesn't throw exceptions.
     */
    public static boolean safeGetBooleanExtra(Intent intent, String name, boolean defaultValue) {
        try {
            return intent.getBooleanExtra(name, defaultValue);
        } catch (Exception e) {
            // Catches un-parceling exceptions.
            return defaultValue;
        }
    }

    /**
     * Just like {@link Intent.getIntExtra()} but doesn't throw exceptions.
     */
    public static int safeGetIntExtra(Intent intent, String name, int defaultValue) {
        try {
            return intent.getIntExtra(name, defaultValue);
        } catch (Exception e) {
            // Catches un-parceling exceptions.
            return defaultValue;
        }
    }

    /**
     * Just like {@link Intent.getLongExtra()} but doesn't throw exceptions.
     */
    public static long safeGetLongExtra(Intent intent, String name, long defaultValue) {
        try {
            return intent.getLongExtra(name, defaultValue);
        } catch (Exception e) {
            // Catches un-parceling exceptions.
            return defaultValue;
        }
    }

    /**
     * Just like {@link Intent.getStringExtra()} but doesn't throw exceptions.
     */
    public static String safeGetStringExtra(Intent intent, String name) {
        try {
            return intent.getStringExtra(name);
        } catch (Exception e) {
            // Catches un-parceling exceptions.
            return null;
        }
    }

    /**
     * Just like {@link Intent.getBundleExtra()} but doesn't throw exceptions.
     */
    public static Bundle safeGetBundleExtra(Intent intent, String name) {
        try {
            return intent.getBundleExtra(name);
        } catch (Exception e) {
            // Catches un-parceling exceptions.
            return null;
        }
    }

    /**
     * Just like {@link Intent.getParcelableExtra()} but doesn't throw exceptions.
     */
    public static <T extends Parcelable> T safeGetParcelableExtra(Intent intent, String name) {
        try {
            return intent.getParcelableExtra(name);
        } catch (Exception e) {
            // Catches un-parceling exceptions.
            return null;
        }
    }

    /**
     * Just like {@link Intent.getStringArrayListExtra()} but doesn't throw exceptions.
     */
    public static ArrayList<String> safeGetStringArrayListExtra(Intent intent, String name) {
        try {
            return intent.getStringArrayListExtra(name);
        } catch (Exception e) {
            // Catches un-parceling exceptions.
            return null;
        }
    }
}

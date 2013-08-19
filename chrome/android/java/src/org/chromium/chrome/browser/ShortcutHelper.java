// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.util.Log;

import org.chromium.base.CalledByNative;
import org.chromium.chrome.browser.BookmarkUtils;

import java.util.UUID;

/**
 * This is a helper class to create shortcuts on the Android home screen.
 */
public class ShortcutHelper {
    public static final String EXTRA_ID = "webapp_id";
    public static final String EXTRA_URL = "webapp_url";

    private static String sWebappActivityPackageName;
    private static String sWebappActivityClassName;

    /**
     * Sets the Activity that gets launched when a webapp shortcut is clicked.
     * @param packageName Package of the Activity to launch.
     * @param className Class name of the Activity to launch.
     */
    public static void setWebappActivityInfo(String packageName, String className) {
        sWebappActivityPackageName = packageName;
        sWebappActivityClassName = className;
    }

    /**
     * Adds a shortcut for the current Tab.
     * @param tab Tab to create a shortcut for.
     */
    public static void addShortcut(TabBase tab) {
        if (sWebappActivityPackageName == null || sWebappActivityClassName == null) {
            Log.e("ShortcutHelper", "ShortcutHelper is uninitialized.  Aborting.");
            return;
        }
        nativeAddShortcut(tab.getNativePtr());
    }

    /**
     * Called when we have to fire an Intent to add a shortcut to the homescreen.
     * If the webpage indicated that it was capable of functioning as a webapp, it is added as a
     * shortcut to a webapp Activity rather than as a general bookmark.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private static void addShortcut(Context context, String url, String title, Bitmap favicon,
            int red, int green, int blue, boolean isWebappCapable) {
        Intent addIntent;
        if (isWebappCapable && !sWebappActivityPackageName.isEmpty()
                && !sWebappActivityClassName.isEmpty()) {
            // Add the shortcut as a launcher icon for a Webapp.
            String id = UUID.randomUUID().toString();

            Intent shortcutIntent = new Intent();
            shortcutIntent.setClassName(sWebappActivityPackageName, sWebappActivityClassName);
            shortcutIntent.putExtra(EXTRA_URL, url);
            shortcutIntent.putExtra(EXTRA_ID, id);

            addIntent = BookmarkUtils.createAddToHomeIntent(context, shortcutIntent, title, favicon,
                            red, green, blue);
        } else {
            // Add the shortcut as a regular bookmark.
            addIntent = BookmarkUtils.createAddToHomeIntent(context, url, title, favicon, red,
                            green, blue);
        }
        context.sendBroadcast(addIntent);
    }

    private static native void nativeAddShortcut(int tabAndroidPtr);
}

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.Bitmap;
import android.graphics.drawable.Icon;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.ShortcutHelper;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.List;

/**
 * This class handles the adding of shortcuts to the Android Home Screen.
 */
public class ChromeShortcutManager {
    private static final String TAG = "ChromeShortcutMgr";

    // There is no public string defining this intent so if Home changes the value, we
    // have to update this string.
    private static final String INSTALL_SHORTCUT = "com.android.launcher.action.INSTALL_SHORTCUT";

    private static ChromeShortcutManager sInstance;

    // True when Android O's ShortcutManager.requestPinShortcut() is supported.
    private boolean mIsRequestPinShortcutSupported;

    // TODO(martiw): Use 'ShortcutInfo' instead of 'Object' below when compileSdk is bumped to O.
    private Object mShortcutManager;

    /**
     * Returns the singleton instance of ChromeShortcutManager, creating it if needed.
     */
    // TODO(martiw): change this to private when ChromeShortcutManagerInternal is gone.
    public static ChromeShortcutManager getInstance() {
        if (sInstance == null) {
            sInstance = new ChromeShortcutManager();
        }
        return sInstance;
    }

    /**
     * Creates an intent that will add a shortcut to the home screen.
     * @param title          Title of the shortcut.
     * @param icon           Image that represents the shortcut.
     * @param shortcutIntent Intent to fire when the shortcut is activated.
     * @return Intent for the shortcut.
     */
    public static Intent createAddToHomeIntent(String title, Bitmap icon, Intent shortcutIntent) {
        Intent i = new Intent(INSTALL_SHORTCUT);
        i.putExtra(Intent.EXTRA_SHORTCUT_INTENT, shortcutIntent);
        i.putExtra(Intent.EXTRA_SHORTCUT_NAME, title);
        i.putExtra(Intent.EXTRA_SHORTCUT_ICON, icon);
        return i;
    }

    // TODO(martiw): Make this private when ChromeShortcutManagerInternal is removed.
    public ChromeShortcutManager() {
        if (BuildInfo.isAtLeastO()) {
            checkIfRequestPinShortcutSupported();
        }
    }

    // TODO(martiw): Use Build.VERSION_CODES.O instead of hardcoded number when it is available.
    @TargetApi(26)
    private void checkIfRequestPinShortcutSupported() {
        // The code in the try-block uses reflection in order to compile as it calls APIs newer than
        // our target version of Android. The equivalent code without reflection is as follows:
        //  mShortcutManager =
        //          ContextUtils.getApplicationContext().getSystemService(ShortcutManager.class);
        //  mIsRequestPinShortcutSupported = mShortcutManager.isRequestPinShortcutSupported();
        // TODO(martiw): Remove the following reflection once compileSdk is bumped to O.
        try {
            Class<?> ShortcutManagerClass = Class.forName("android.content.pm.ShortcutManager");
            mShortcutManager =
                    ContextUtils.getApplicationContext().getSystemService(ShortcutManagerClass);

            Method isRequestPinShortcutSupported =
                    ShortcutManagerClass.getMethod("isRequestPinShortcutSupported");
            mIsRequestPinShortcutSupported =
                    (boolean) isRequestPinShortcutSupported.invoke(mShortcutManager);
        } catch (ClassNotFoundException | NoSuchMethodException | InvocationTargetException
                | IllegalAccessException e) {
            Log.e(TAG, "Error checking if RequestPinShortcut is supported:", e);
        }
    }

    /**
     * Add a shortcut to the home screen.
     * @param title          Title of the shortcut.
     * @param icon           Image that represents the shortcut.
     * @param shortcutIntent Intent to fire when the shortcut is activated.
     */
    public void addShortcutToHomeScreen(String title, Bitmap icon, Intent shortcutIntent) {
        if (mIsRequestPinShortcutSupported) {
            addShortcutWithShortcutManager(title, icon, shortcutIntent);
            return;
        }
        Intent intent = createAddToHomeIntent(title, icon, shortcutIntent);
        ContextUtils.getApplicationContext().sendBroadcast(intent);
    }

    // TODO(martiw): Use Build.VERSION_CODES.O instead of hardcoded number when it is available.
    @TargetApi(26)
    private void addShortcutWithShortcutManager(String title, Bitmap icon, Intent shortcutIntent) {
        String id = shortcutIntent.getStringExtra(ShortcutHelper.EXTRA_ID);
        Context context = ContextUtils.getApplicationContext();

        // The code in the try-block uses reflection in order to compile as it calls APIs newer than
        // our compileSdkVersion of Android. The equivalent code without reflection looks like this:
        //  ShortcutInfo shortcutInfo = new ShortcutInfo.Builder(context, id)
        //                                  .setShortLabel(title)
        //                                  .setLongLabel(title)
        //                                  .setIcon(Icon.createWithBitmap(icon))
        //                                  .setIntent(shortcutIntent)
        //                                  .build();
        //  mShortcutManager.requestPinShortcut(shortcutInfo, null);
        // TODO(martiw): Remove the following reflection once compileSdk is bumped to O.
        try {
            Class<?> builderClass = Class.forName("android.content.pm.ShortcutInfo$Builder");
            Constructor<?> builderConstructor =
                    builderClass.getDeclaredConstructor(Context.class, String.class);
            Object shortcutBuilder = builderConstructor.newInstance(context, id);

            Method setShortLabel = builderClass.getMethod("setShortLabel", CharSequence.class);
            setShortLabel.invoke(shortcutBuilder, title);

            Method setLongLabel = builderClass.getMethod("setLongLabel", CharSequence.class);
            setLongLabel.invoke(shortcutBuilder, title);

            Method setIcon = builderClass.getMethod("setIcon", Icon.class);
            setIcon.invoke(shortcutBuilder, Icon.createWithBitmap(icon));

            Method setIntent = builderClass.getMethod("setIntent", Intent.class);
            setIntent.invoke(shortcutBuilder, shortcutIntent);

            Method build = builderClass.getMethod("build");
            Object shortcutInfo = build.invoke(shortcutBuilder);

            Class<?> ShortcutInfoClass = Class.forName("android.content.pm.ShortcutInfo");
            Method requestPinShortcut = mShortcutManager.getClass().getMethod(
                    "requestPinShortcut", ShortcutInfoClass, IntentSender.class);
            requestPinShortcut.invoke(mShortcutManager, shortcutInfo, null);
        } catch (ClassNotFoundException | NoSuchMethodException | InstantiationException
                | InvocationTargetException | IllegalAccessException e) {
            Log.e(TAG, "Error adding shortcut with ShortcutManager:", e);
        }
    }

    // TODO(crbug.com/635567): Fix this properly.
    @SuppressLint("WrongConstant")
    public boolean canAddShortcutToHomescreen() {
        if (mIsRequestPinShortcutSupported) return true;
        PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
        Intent i = new Intent(INSTALL_SHORTCUT);
        List<ResolveInfo> receivers =
                pm.queryBroadcastReceivers(i, PackageManager.GET_INTENT_FILTERS);
        return !receivers.isEmpty();
    }

    public boolean shouldShowToastWhenAddingShortcut() {
        return !mIsRequestPinShortcutSupported;
    }
}

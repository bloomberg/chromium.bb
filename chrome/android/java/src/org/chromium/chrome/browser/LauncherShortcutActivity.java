// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ShortcutInfo;
import android.content.pm.ShortcutManager;
import android.graphics.drawable.Icon;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.StrictMode;
import android.provider.Browser;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * A helper activity for routing launcher shortcut intents.
 */
public class LauncherShortcutActivity extends Activity {
    private static final String ACTION_OPEN_NEW_TAB = "chromium.shortcut.action.OPEN_NEW_TAB";
    private static final String ACTION_OPEN_NEW_INCOGNITO_TAB =
            "chromium.shortcut.action.OPEN_NEW_INCOGNITO_TAB";
    private static final String DYNAMIC_OPEN_NEW_INCOGNITO_TAB_ID =
            "dynamic-new-incognito-tab-shortcut";
    private static final String INCOGNITO_SHORTCUT_ADDED_PREF = "incognito-shortcut-added";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        String intentAction = getIntent().getAction();

        // Exit early if the original intent action isn't for opening a new tab.
        if (!intentAction.equals(ACTION_OPEN_NEW_TAB)
                && !intentAction.equals(ACTION_OPEN_NEW_INCOGNITO_TAB)) {
            finish();
            return;
        }

        Intent newIntent = new Intent();
        newIntent.setAction(Intent.ACTION_VIEW);
        newIntent.setData(Uri.parse(UrlConstants.NTP_URL));
        newIntent.setClass(this, ChromeLauncherActivity.class);
        newIntent.putExtra(IntentHandler.EXTRA_INVOKED_FROM_SHORTCUT, true);
        newIntent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        newIntent.putExtra(Browser.EXTRA_APPLICATION_ID, getPackageName());
        IntentHandler.addTrustedIntentExtras(newIntent);

        if (intentAction.equals(ACTION_OPEN_NEW_INCOGNITO_TAB)) {
            newIntent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        }

        // This system call is often modified by OEMs and not actionable. http://crbug.com/619646.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        try {
            startActivity(newIntent);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }

        finish();
    }

    /**
     * Adds or removes the "New incognito tab" launcher shortcut based on whether incognito mode
     * is enabled.
     * @param context The context used to retrieve the system {@link ShortcutManager}.
     */
    public static void updateIncognitoShortcut(Context context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N_MR1) return;

        SharedPreferences preferences = ContextUtils.getAppSharedPreferences();
        if (!preferences.getBoolean(INCOGNITO_SHORTCUT_ADDED_PREF, false)
                && PrefServiceBridge.getInstance().isIncognitoModeEnabled()) {
            boolean success = LauncherShortcutActivity.addIncognitoLauncherShortcut(context);
            preferences.edit().putBoolean(INCOGNITO_SHORTCUT_ADDED_PREF, success).apply();
        } else if (preferences.getBoolean(INCOGNITO_SHORTCUT_ADDED_PREF, false)
                && !PrefServiceBridge.getInstance().isIncognitoModeEnabled()) {
            LauncherShortcutActivity.removeIncognitoLauncherShortcut(context);
            preferences.edit().putBoolean(INCOGNITO_SHORTCUT_ADDED_PREF, false).apply();
        }
    }

    /**
     * Adds a "New incognito tab" dynamic launcher shortcut.
     * @param context The context used to retrieve the system {@link ShortcutManager}.
     * @return True if addint the shortcut has succeeded. False if the call fails due to rate
     *         limiting. See {@link ShortcutManager#addDynamicShortcuts}.
     */
    @TargetApi(Build.VERSION_CODES.N_MR1)
    private static boolean addIncognitoLauncherShortcut(Context context) {
        Intent intent = new Intent(LauncherShortcutActivity.ACTION_OPEN_NEW_INCOGNITO_TAB);
        intent.setPackage(context.getPackageName());
        intent.setClass(context, LauncherShortcutActivity.class);

        ShortcutInfo shortcut =
                new ShortcutInfo.Builder(context, DYNAMIC_OPEN_NEW_INCOGNITO_TAB_ID)
                        .setShortLabel(context.getResources().getString(
                                R.string.accessibility_tabstrip_incognito_identifier))
                        .setLongLabel(
                                context.getResources().getString(R.string.menu_new_incognito_tab))
                        .setIcon(Icon.createWithResource(context, R.drawable.shortcut_incognito))
                        .setIntent(intent)
                        .build();

        ShortcutManager shortcutManager = context.getSystemService(ShortcutManager.class);
        return shortcutManager.addDynamicShortcuts(Arrays.asList(shortcut));
    }

    /**
     * Removes the dynamic "New incognito tab" launcher shortcut.
     * @param context The context used to retrieve the system {@link ShortcutManager}.
     */
    @TargetApi(Build.VERSION_CODES.N_MR1)
    private static void removeIncognitoLauncherShortcut(Context context) {
        List<String> shortcutList = new ArrayList<>();
        shortcutList.add(DYNAMIC_OPEN_NEW_INCOGNITO_TAB_ID);

        ShortcutManager shortcutManager = context.getSystemService(ShortcutManager.class);
        shortcutManager.disableShortcuts(shortcutList);
        shortcutManager.removeDynamicShortcuts(shortcutList);
    }
}

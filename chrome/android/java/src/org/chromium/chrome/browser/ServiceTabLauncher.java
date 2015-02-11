// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.net.Uri;
import android.provider.Browser;
import android.util.Log;

import org.chromium.base.CalledByNative;
import org.chromium.ui.WindowOpenDisposition;

/**
 * Tab Launcher to be used to launch new tabs from background Android Services, when it is not
 * known whether an activity is available. It will send an intent to launch the activity.
 *
 * TODO(peter): Have the activity confirm that the tab has been launched.
 */
public class ServiceTabLauncher {
    private static final String TAG = ServiceTabLauncher.class.getSimpleName();

    private static final String BROWSER_ACTIVITY_KEY =
            "org.chromium.chrome.browser.BROWSER_ACTIVITY";
    private static final String EXTRA_OPEN_NEW_INCOGNITO_TAB =
            "com.google.android.apps.chrome.EXTRA_OPEN_NEW_INCOGNITO_TAB";

    private final long mNativeServiceTabLauncher;
    private final Context mContext;

    @CalledByNative
    private static ServiceTabLauncher create(long nativeServiceTabLauncher, Context context) {
        return new ServiceTabLauncher(nativeServiceTabLauncher, context);
    }

    private ServiceTabLauncher(long nativeServiceTabLauncher, Context context) {
        mNativeServiceTabLauncher = nativeServiceTabLauncher;
        mContext = context;
    }

    /**
     * Launches the browser activity and launches a tab for |url|.
     *
     * @param url The URL which should be launched in a tab.
     * @param disposition The disposition requested by the navigation source.
     * @param incognito Whether the tab should be launched in incognito mode.
     */
    @CalledByNative
    private void launchTab(String url, int disposition, boolean incognito) {
        Intent intent = new Intent(mContext, getBrowserActivityClassFromManifest());
        intent.setAction(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setData(Uri.parse(url));

        switch (disposition) {
            case WindowOpenDisposition.NEW_WINDOW:
            case WindowOpenDisposition.NEW_POPUP:
            case WindowOpenDisposition.NEW_FOREGROUND_TAB:
            case WindowOpenDisposition.NEW_BACKGROUND_TAB:
                // The browser should attempt to create a new tab.
                intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
                break;
            default:
                // The browser should attempt to re-use an existing tab.
                break;
        }

        if (incognito) intent.putExtra(EXTRA_OPEN_NEW_INCOGNITO_TAB, true);

        // TODO(peter): Add the referrer information when applicable.

        mContext.startActivity(intent);
    }

    /**
     * Reads the BROWSER_ACTIVITY name from the meta data in the Android Manifest file.
     *
     * @return Class for the browser activity to use when launching tabs.
     */
    private Class<?> getBrowserActivityClassFromManifest() {
        try {
            ApplicationInfo info = mContext.getPackageManager().getApplicationInfo(
                    mContext.getPackageName(), PackageManager.GET_META_DATA);
            String className = info.metaData.getString(BROWSER_ACTIVITY_KEY);

            return Class.forName(className);
        } catch (NameNotFoundException e) {
            Log.e(TAG, "Context.getPackage() refers to an invalid package name.");
        } catch (ClassNotFoundException e) {
            Log.e(TAG, "Invalid value for BROWSER_ACTIVITY in the Android manifest file.");
        }

        return null;
    }
}

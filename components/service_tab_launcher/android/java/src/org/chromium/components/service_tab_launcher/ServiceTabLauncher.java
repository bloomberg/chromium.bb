// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.service_tab_launcher;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.util.Log;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.content_public.browser.WebContents;

/**
 * Tab Launcher to be used to launch new tabs from background Android Services, when it is not
 * known whether an activity is available. It will send an intent to launch the activity.
 *
 * TODO(peter): Have the activity confirm that the tab has been launched.
 */
public abstract class ServiceTabLauncher {
    private static final String TAG = ServiceTabLauncher.class.getSimpleName();
    private static final String SERVICE_TAB_LAUNCHER_KEY =
            "org.chromium.components.service_tab_launcher.SERVICE_TAB_LAUNCHER";

    // Name of the extra containing the Id of a tab launch request id.
    public static final String LAUNCH_REQUEST_ID_EXTRA =
            "org.chromium.components.service_tab_launcher.ServiceTabLauncher.LAUNCH_REQUEST_ID";

    private static ServiceTabLauncher sInstance;

    /**
     * Launches the browser activity and launches a tab for |url|.
     *
     * @param context The context using which the URL is being loaded.
     * @param requestId Id of the request for launching this tab.
     * @param incognito Whether the tab should be launched in incognito mode.
     * @param url The URL which should be launched in a tab.
     * @param disposition The disposition requested by the navigation source.
     * @param referrerUrl URL of the referrer which is opening the page.
     * @param referrerPolicy The referrer policy to consider when applying the referrer.
     * @param extraHeaders Extra headers to apply when requesting the tab's URL.
     * @param postData Post-data to include in the tab URL's request body.
     */
    @CalledByNative
    public abstract void launchTab(Context context, int requestId, boolean incognito, String url,
                                   int disposition, String referrerUrl, int referrerPolicy,
                                   String extraHeaders, byte[] postData);

    /**
     * Returns the active instance of the ServiceTabLauncher. If none exists, a meta-data key will
     * be read from the AndroidManifest.xml file to determine the class which implements the
     * service tab launcher activity. If that fails, NULL will be returned instead.
     *
     * We need to do this run-around because the ServiceTabLauncher must be available from
     * background services, where no activity may be alive. Furthermore, no browser shell code has
     * to be involved in the process at all, which means that there's no common location where we
     * can inject a delegate in the start-up paths.
     *
     * @param Context The application context for the running service.
     */
    @CalledByNative
    private static ServiceTabLauncher getInstance(Context context) throws Exception {
        if (sInstance == null) {
            Class<?> implementation = getServiceTabLauncherClassFromManifest(context);
            if (implementation != null) {
                sInstance = (ServiceTabLauncher) implementation.newInstance();
            }
        }

        return sInstance;
    }

    /**
     * Reads the SERVICE_TAB_LAUNCHER_KEY from the manifest and returns the Class it
     * refers to. If the class cannot be found, NULL will be returned instead.
     *
     * @param context The application context used to get the package name and manager.
     */
    private static Class<?> getServiceTabLauncherClassFromManifest(Context context) {
        try {
            ApplicationInfo info = context.getPackageManager().getApplicationInfo(
                    context.getPackageName(), PackageManager.GET_META_DATA);
            String className = info.metaData.getString(SERVICE_TAB_LAUNCHER_KEY);

            return Class.forName(className);
        } catch (NameNotFoundException e) {
            Log.e(TAG, "Context.getPackage() refers to an invalid package name.");
        } catch (ClassNotFoundException e) {
            Log.e(TAG, "Invalid value for SERVICE_TAB_LAUNCHER in the Android manifest file.");
        }

        return null;
    }

    /**
     * To be called by the activity when the WebContents for |requestId| has been created, or has
     * been recycled from previous use. The |webContents| must not yet have started provisional
     * load for the main frame.
     *
     * @param requestId Id of the tab launching request which has been fulfilled.
     * @param webContents The WebContents instance associated with this request.
     */
    public static void onWebContentsForRequestAvailable(int requestId, WebContents webContents) {
        nativeOnWebContentsForRequestAvailable(requestId, webContents);
    }

    private static native void nativeOnWebContentsForRequestAvailable(
            int requestId, WebContents webContents);
}

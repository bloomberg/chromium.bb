// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.Intent;
import android.os.AsyncTask;

import org.chromium.base.CommandLine;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.AsyncTabCreationParams;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.chrome.browser.webapps.WebappDataStorage;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.chrome.browser.webapps.WebappRegistry.FetchWebappDataStorageCallback;
import org.chromium.components.service_tab_launcher.ServiceTabLauncher;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.base.PageTransition;
import org.chromium.webapk.lib.client.NavigationClient;
import org.chromium.webapk.lib.client.WebApkValidator;

/**
 * Service Tab Launcher implementation for Chrome. Provides the ability for Android Services
 * running in Chrome to launch URLs, without having access to an activity.
 *
 * This class is referred to from the ServiceTabLauncher implementation in Chromium using a
 * meta-data value in the Android manifest file. The ServiceTabLauncher class has more
 * documentation about why this is necessary.
 *
 * URLs within the scope of a recently launched standalone-capable web app on the Android home
 * screen are launched in the standalone web app frame.
 *
 * TODO(peter): after upstreaming, merge this with ServiceTabLauncher and remove reflection calls
 *              in ServiceTabLauncher.
 */
public class ChromeServiceTabLauncher extends ServiceTabLauncher {
    @Override
    public void launchTab(final Context context, final int requestId, final boolean incognito,
            final String url, final int disposition, final String referrerUrl,
            final int referrerPolicy, final String extraHeaders, final byte[] postData) {
        final TabDelegate tabDelegate = new TabDelegate(incognito);

        // 1. Launch WebAPK if one matches the target URL.
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.ENABLE_WEBAPK)) {
            String webApkPackageName = WebApkValidator.queryWebApkPackage(context, url);
            if (webApkPackageName != null) {
                NavigationClient.launchWebApk(context, webApkPackageName, url);
                return;
            }
        }

        // 2. Launch WebappActivity if one matches the target URL and was opened recently.
        // Otherwise, open the URL in a tab.
        FetchWebappDataStorageCallback callback = new FetchWebappDataStorageCallback() {
            @Override
            public void onWebappDataStorageRetrieved(final WebappDataStorage storage) {
                // If we do not find a WebappDataStorage corresponding to this URL, or if it hasn't
                // been opened recently enough, open the URL in a tab.
                if (storage == null || !storage.wasLaunchedRecently()) {
                    LoadUrlParams loadUrlParams = new LoadUrlParams(url, PageTransition.LINK);
                    loadUrlParams.setPostData(postData);
                    loadUrlParams.setVerbatimHeaders(extraHeaders);
                    loadUrlParams.setReferrer(new Referrer(referrerUrl, referrerPolicy));

                    AsyncTabCreationParams asyncParams = new AsyncTabCreationParams(loadUrlParams,
                            requestId);
                    tabDelegate.createNewTab(asyncParams, TabLaunchType.FROM_CHROME_UI,
                            Tab.INVALID_TAB_ID);
                } else {
                    // The URL is within the scope of a recently launched standalone-capable web app
                    // on the home screen, so open it a standalone web app frame. An AsyncTask is
                    // used because WebappDataStorage.createWebappLaunchIntent contains a Bitmap
                    // decode operation and should not be run on the UI thread.
                    //
                    // This currently assumes that the only source is notifications; any future use
                    // which adds a different source will need to change this.
                    new AsyncTask<Void, Void, Intent>() {
                        @Override
                        protected final Intent doInBackground(Void... nothing) {
                            return storage.createWebappLaunchIntent();
                        }

                        @Override
                        protected final void onPostExecute(Intent intent) {
                            intent.putExtra(ShortcutHelper.EXTRA_SOURCE,
                                    ShortcutSource.NOTIFICATION);
                            tabDelegate.createNewStandaloneFrame(intent);
                        }
                    }.execute();
                }
            }
        };
        WebappRegistry.getWebappDataStorageForUrl(context, url, callback);
    }
}

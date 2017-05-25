// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.tab.BrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.webapk.lib.client.WebApkNavigationClient;

/**
 * A {@link TabDelegateFactory} class to be used in all {@link Tab} instances owned by a
 * {@link FullScreenActivity}.
 */
public class WebappDelegateFactory extends FullScreenDelegateFactory {
    private static class WebappWebContentsDelegateAndroid extends TabWebContentsDelegateAndroid {
        private final WebappActivity mActivity;

        public WebappWebContentsDelegateAndroid(WebappActivity activity, Tab tab) {
            super(tab);
            mActivity = activity;
        }

        @Override
        public void activateContents() {
            // Create an Intent that will be fired toward the WebappLauncherActivity, which in turn
            // will fire an Intent to launch the correct WebappActivity or WebApkActivity. On L+
            // this could probably be changed to call AppTask.moveToFront(), but for backwards
            // compatibility we relaunch it the hard way.
            String startUrl = mActivity.getWebappInfo().uri().toString();

            String webApkPackageName = mActivity.getWebappInfo().webApkPackageName();
            if (!TextUtils.isEmpty(webApkPackageName)) {
                Intent intent = WebApkNavigationClient.createLaunchWebApkIntent(
                        webApkPackageName, startUrl, false /* forceNavigation */);
                IntentUtils.safeStartActivity(ContextUtils.getApplicationContext(), intent);
                return;
            }

            Intent intent = new Intent();
            intent.setAction(WebappLauncherActivity.ACTION_START_WEBAPP);
            intent.setPackage(mActivity.getPackageName());
            mActivity.getWebappInfo().setWebappIntentExtras(intent);

            intent.putExtra(
                    ShortcutHelper.EXTRA_MAC, ShortcutHelper.getEncodedMac(mActivity, startUrl));
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            IntentUtils.safeStartActivity(ContextUtils.getApplicationContext(), intent);
        }
    }

    private final WebappActivity mActivity;

    public WebappDelegateFactory(WebappActivity activity) {
        mActivity = activity;
    }

    @Override
    public TabWebContentsDelegateAndroid createWebContentsDelegate(Tab tab) {
        return new WebappWebContentsDelegateAndroid(mActivity, tab);
    }

    @Override
    public BrowserControlsVisibilityDelegate createBrowserControlsVisibilityDelegate(Tab tab) {
        return new WebappBrowserControlsDelegate(mActivity, tab);
    }
}

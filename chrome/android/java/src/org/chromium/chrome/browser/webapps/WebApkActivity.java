// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.banners.AppBannerManager;
import org.chromium.chrome.browser.externalnav.ExternalNavigationParams;
import org.chromium.chrome.browser.tab.InterceptNavigationDelegateImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabRedirectHandler;
import org.chromium.components.navigation_interception.NavigationParams;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.webapk.lib.client.WebApkServiceConnectionManager;

/**
 * An Activity is designed for WebAPKs (native Android apps) and displays a webapp in a nearly
 * UI-less Chrome.
 */
public class WebApkActivity extends WebappActivity {
    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        // We could bring a WebAPK hosted WebappActivity to foreground and navigate it to a
        // different URL. For example, WebAPK "foo" is launched and navigates to
        // "www.foo.com/foo". In Chrome, user clicks a link "www.foo.com/bar" in Google search
        // results. After clicking the link, WebAPK "foo" is brought to foreground, and
        // loads the page of "www.foo.com/bar" at the same time.
        // The extra {@link ShortcutHelper.EXTRA_URL} provides the URL that the WebAPK will
        // navigate to.
        String overrideUrl = intent.getStringExtra(ShortcutHelper.EXTRA_URL);
        if (overrideUrl != null && isInitialized()
                && !overrideUrl.equals(getActivityTab().getUrl())) {
            getActivityTab().loadUrl(
                    new LoadUrlParams(overrideUrl, PageTransition.AUTO_TOPLEVEL));
        }
    }

    @Override
    protected void initializeSplashScreenWidgets(final int backgroundColor) {
        // TODO(hanxi): Removes this function and use {@link WebApkActivity}'s implementation
        // when WebAPKs are registered in WebappRegistry.
        initializeSplashScreenWidgets(backgroundColor, null);
    }

    @Override
    protected TabDelegateFactory createTabDelegateFactory() {
        return new WebappDelegateFactory(this) {
            @Override
            public InterceptNavigationDelegateImpl createInterceptNavigationDelegate(Tab tab) {
                return new InterceptNavigationDelegateImpl(tab) {
                    @Override
                    public ExternalNavigationParams.Builder buildExternalNavigationParams(
                            NavigationParams navigationParams,
                            TabRedirectHandler tabRedirectHandler, boolean shouldCloseTab) {
                        ExternalNavigationParams.Builder builder =
                                super.buildExternalNavigationParams(
                                        navigationParams, tabRedirectHandler, shouldCloseTab);
                        builder.setIsWebApk(true);
                        return builder;
                    }
                };
            }

            @Override
            public AppBannerManager createAppBannerManager(Tab tab) {
                // Do not show app banners for WebAPKs regardless of the current page URL.
                // A WebAPK can display a page outside of its WebAPK scope if a page within the
                // WebAPK scope navigates via JavaScript while the WebAPK is in the background.
                return null;
            }
        };
    }

    public void onStop() {
        super.onStop();
        String packageName = getWebappInfo().webApkPackageName();
        WebApkServiceConnectionManager.getInstance().disconnect(
                ContextUtils.getApplicationContext(), packageName);
    }
}

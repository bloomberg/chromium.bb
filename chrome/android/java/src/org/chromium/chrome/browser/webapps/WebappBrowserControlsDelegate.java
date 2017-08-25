// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.text.TextUtils;

import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.components.security_state.ConnectionSecurityLevel;

class WebappBrowserControlsDelegate extends TabStateBrowserControlsVisibilityDelegate {
    private final WebappActivity mActivity;

    public WebappBrowserControlsDelegate(WebappActivity activity, Tab tab) {
        super(tab);
        mActivity = activity;
    }

    @Override
    public boolean canShowBrowserControls() {
        if (!super.canShowBrowserControls()) return false;

        return shouldShowBrowserControls(
                mActivity.getWebappInfo(), mTab.getUrl(), mTab.getSecurityLevel());
    }

    @Override
    public boolean canAutoHideBrowserControls() {
        return canAutoHideBrowserControls(mTab.getSecurityLevel());
    }

    boolean canAutoHideBrowserControls(int securityLevel) {
        // Allow auto-hiding browser controls unless they are shown because of low security level.
        return !shouldShowBrowserControlsForSecurityLevel(securityLevel);
    }

    /**
     * Returns whether the browser controls should be shown when a webapp is navigated to
     * {@code url} given the page's security level.
     * @param info data of a Web App
     * @param url The webapp's current URL
     * @param securityLevel The security level for the webapp's current URL.
     * @return Whether the browser controls should be shown for {@code url}.
     */
    boolean shouldShowBrowserControls(WebappInfo info, String url, int securityLevel) {
        // Do not show browser controls when URL is not ready yet.
        if (TextUtils.isEmpty(url)) return false;

        return shouldShowBrowserControlsForUrl(info, url)
                || shouldShowBrowserControlsForDisplayMode(info)
                || shouldShowBrowserControlsForSecurityLevel(securityLevel);
    }

    /**
     * Returns whether the browser controls should be shown when a webapp is navigated to
     * {@code url}.
     */
    protected boolean shouldShowBrowserControlsForUrl(WebappInfo info, String url) {
        return !UrlUtilities.sameDomainOrHost(info.uri().toString(), url, true);
    }

    private boolean shouldShowBrowserControlsForSecurityLevel(int securityLevel) {
        return securityLevel == ConnectionSecurityLevel.DANGEROUS;
    }

    private boolean shouldShowBrowserControlsForDisplayMode(WebappInfo info) {
        return info.displayMode() != WebDisplayMode.STANDALONE
                && info.displayMode() != WebDisplayMode.FULLSCREEN;
    }
}

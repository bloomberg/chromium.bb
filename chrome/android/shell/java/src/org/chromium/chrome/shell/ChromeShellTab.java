// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.shell;

import android.content.Context;
import android.text.TextUtils;

import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.UrlUtilities;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulator;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.content.browser.ContentViewClient;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.base.WindowAndroid;

/**
 * ChromeShell's implementation of a tab. This mirrors how Chrome for Android subclasses
 * and extends {@link Tab}.
 */
public class ChromeShellTab extends Tab {
    // Tab state
    private boolean mIsLoading;
    private boolean mIsFullscreen = false;
    private TabManager mTabManager;

    /**
     * @param context           The Context the view is running in.
     * @param url               The URL to start this tab with.
     * @param window            The WindowAndroid should represent this tab.
     * @param contentViewClient The client for the {@link ContentViewCore}s of this Tab.
     */
    public ChromeShellTab(Context context, String url, WindowAndroid window,
            ContentViewClient contentViewClient, TabManager tabManager) {
        super(false, context, window);
        initialize();
        initContentViewCore();
        setContentViewClient(contentViewClient);
        loadUrlWithSanitization(url);
        mTabManager = tabManager;
    }

    /**
     * @return Whether or not the tab is currently loading.
     */
    public boolean isLoading() {
        return mIsLoading;
    }

    /**
     * Navigates this Tab to a sanitized version of {@code url}.
     * @param url The potentially unsanitized URL to navigate to.
     * @param postData Optional data to be sent via POST.
     */
    public void loadUrlWithSanitization(String url, byte[] postData) {
        if (url == null) return;

        // Sanitize the URL.
        url = UrlUtilities.fixupUrl(url);

        // Invalid URLs will just return empty.
        if (TextUtils.isEmpty(url)) return;

        WebContents webContents = getWebContents();
        NavigationController navigationController = webContents.getNavigationController();
        if (TextUtils.equals(url, webContents.getUrl())) {
            navigationController.reload(true);
        } else {
            if (postData == null) {
                navigationController.loadUrl(new LoadUrlParams(url));
            } else {
                navigationController.loadUrl(LoadUrlParams.createLoadHttpPostParams(url, postData));
            }
        }
    }

    /**
     * Navigates this Tab to a sanitized version of {@code url}.
     * @param url The potentially unsanitized URL to navigate to.
     */
    public void loadUrlWithSanitization(String url) {
        loadUrlWithSanitization(url, null);
    }

    @Override
    protected TabChromeWebContentsDelegateAndroid createWebContentsDelegate() {
        return new ChromeShellTabChromeWebContentsDelegateAndroid();
    }

    @Override
    protected ContextMenuPopulator createContextMenuPopulator() {
        return new ChromeContextMenuPopulator(new TabChromeContextMenuItemDelegate() {
            @Override
            public void onOpenImageUrl(String url, Referrer referrer) {
                loadUrlWithSanitization(url);
            }

            @Override
            public void onOpenInNewTab(String url, Referrer referrer) {
                mTabManager.createTab(url, TabLaunchType.FROM_LINK);
            }

            @Override
            public void onOpenImageInNewTab(String url, Referrer referrer) {
                mTabManager.createTab(url, TabLaunchType.FROM_LINK);
            }
        });
    }

    private class ChromeShellTabChromeWebContentsDelegateAndroid
            extends TabChromeWebContentsDelegateAndroid {
        @Override
        public void onLoadStarted() {
            mIsLoading = true;
        }

        @Override
        public void onLoadStopped() {
            mIsLoading = false;
        }

        @Override
        public void toggleFullscreenModeForTab(boolean enterFullscreen) {
            mIsFullscreen = enterFullscreen;
            super.toggleFullscreenModeForTab(enterFullscreen);
        }

        @Override
        public boolean isFullscreenForTabOrPending() {
            return mIsFullscreen;
        }

        @Override
        public void webContentsCreated(long sourceWebContents, long openerRenderFrameId,
                String frameName, String targetUrl, long newWebContents) {
            mTabManager.createTab(targetUrl, TabLaunchType.FROM_LINK);
        }
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.metrics.WebApkUma;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.net.NetError;
import org.chromium.net.NetworkChangeNotifier;

/**
 * Displays error dialog on top of splash screen if there is a network error while loading the
 * start URL.
 */
public class WebApkSplashNetworkErrorObserver extends EmptyTabObserver {
    // No error.
    public static final int ERROR_OK = 0;

    private WebappSplashDelegate mDelegate;

    private String mWebApkName;

    private boolean mDidShowNetworkErrorDialog;

    /** Indicates whether reloading is allowed. */
    private boolean mAllowReloads;

    public WebApkSplashNetworkErrorObserver(WebappSplashDelegate delegate, String webApkName) {
        mDelegate = delegate;
        mWebApkName = webApkName;
    }

    @Override
    public void onDidFinishNavigation(final Tab tab, final String url, boolean isInMainFrame,
            boolean isErrorPage, boolean hasCommitted, boolean isSameDocument,
            boolean isFragmentNavigation, Integer pageTransition, int errorCode,
            int httpStatusCode) {
        if (!isInMainFrame) return;

        switch (errorCode) {
            case ERROR_OK:
                mDelegate.hideWebApkNetworkErrorDialog();
                break;
            case NetError.ERR_NETWORK_CHANGED:
                onNetworkChanged(tab);
                break;
            default:
                onNetworkError(tab, errorCode);
                break;
        }
        WebApkUma.recordNetworkErrorWhenLaunch(-errorCode);
    }

    private void onNetworkChanged(Tab tab) {
        if (!mAllowReloads) return;

        // It is possible that we get {@link NetError.ERR_NETWORK_CHANGED} during the first
        // reload after the device is online. The navigation will fail until the next auto
        // reload fired by {@link NetErrorHelperCore}. We call reload explicitly to reduce the
        // waiting time.
        tab.reloadIgnoringCache();
        mAllowReloads = false;
    }

    private void onNetworkError(final Tab tab, int errorCode) {
        if (tab.getActivity() == null) return;

        // Do not show the network error dialog more than once (e.g. if the user backed out of
        // the dialog).
        if (mDidShowNetworkErrorDialog) return;

        mDidShowNetworkErrorDialog = true;

        final NetworkChangeNotifier.ConnectionTypeObserver observer =
                new NetworkChangeNotifier.ConnectionTypeObserver() {
                    @Override
                    public void onConnectionTypeChanged(int connectionType) {
                        if (!NetworkChangeNotifier.isOnline()) return;

                        NetworkChangeNotifier.removeConnectionTypeObserver(this);
                        tab.reloadIgnoringCache();
                        // One more reload is allowed after the network connection is back.
                        mAllowReloads = true;
                    }
                };

        NetworkChangeNotifier.addConnectionTypeObserver(observer);
        mDelegate.showWebApkNetworkErrorDialog(generateNetworkErrorWebApkDialogMessage(errorCode));
    }

    /** Generates network error dialog message for the given error code. */
    private String generateNetworkErrorWebApkDialogMessage(int errorCode) {
        Context context = ContextUtils.getApplicationContext();
        switch (errorCode) {
            case NetError.ERR_INTERNET_DISCONNECTED:
                return context.getString(R.string.webapk_offline_dialog, mWebApkName);
            case NetError.ERR_TUNNEL_CONNECTION_FAILED:
                return context.getString(
                        R.string.webapk_network_error_message_tunnel_connection_failed);
            default:
                return context.getString(R.string.webapk_cannot_connect_to_site);
        }
    }
}

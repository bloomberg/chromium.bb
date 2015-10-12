// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.text.TextUtils;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.contextmenu.ContextMenuItemDelegate;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.PageTransition;

import java.util.Locale;

/**
 * A default {@link ContextMenuItemDelegate} that supports the context menu functionality in Tab.
 */
public class TabContextMenuItemDelegate implements ContextMenuItemDelegate {
    public static final String PAGESPEED_PASSTHROUGH_HEADERS =
            "Chrome-Proxy: pass-through\nCache-Control: no-cache";

    private final Clipboard mClipboard;
    private final Tab mTab;
    private final ChromeActivity mActivity;

    /**
     * The data reduction proxy was in use on the last page load if true.
     */
    protected boolean mUsedSpdyProxy;

    /**
     * The data reduction proxy was in pass through mode on the last page load if true.
     */
    protected boolean mUsedSpdyProxyWithPassthrough;

    /**
     * The last page load had request headers indicating that the data reduction proxy should
     * be put in pass through mode, if true.
     */
    protected boolean mLastPageLoadHasSpdyProxyPassthroughHeaders;

    /**
     * Builds a {@link TabContextMenuItemDelegate} instance.
     */
    public TabContextMenuItemDelegate(Tab tab, ChromeActivity activity) {
        mTab = tab;
        mActivity = activity;
        mClipboard = new Clipboard(mTab.getApplicationContext());
        mTab.addObserver(new EmptyTabObserver() {
            @Override
            public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
                super.onLoadUrl(tab, params, loadType);
                // The data reduction proxy can only be set to pass through mode via loading an
                // image in a new tab. We squirrel away whether pass through mode was set, and check
                // it in: @see TabWebContentsDelegateAndroid#onLoadStopped()
                mLastPageLoadHasSpdyProxyPassthroughHeaders = false;
                if (TextUtils.equals(params.getVerbatimHeaders(), PAGESPEED_PASSTHROUGH_HEADERS)) {
                    mLastPageLoadHasSpdyProxyPassthroughHeaders = true;
                }
            }

            @Override
            public void onPageLoadFinished(Tab tab) {
                super.onPageLoadFinished(tab);
                maybeSetDataReductionProxyUsed();
            }
        });
    }

    @Override
    public boolean isIncognito() {
        return mTab.isIncognito();
    }

    @Override
    public boolean isIncognitoSupported() {
        return PrefServiceBridge.getInstance().isIncognitoModeEnabled();
    }

    @Override
    public boolean canLoadOriginalImage() {
        return mUsedSpdyProxy && !mUsedSpdyProxyWithPassthrough;
    }

    @Override
    public boolean isDataReductionProxyEnabledForURL(String url) {
        return isSpdyProxyEnabledForUrl(url);
    }

    @Override
    public boolean startDownload(String url, boolean isLink) {
        return !isLink || !mTab.shouldInterceptContextMenuDownload(url);
    }

    @Override
    public void onSaveToClipboard(String text, int clipboardType) {
        mClipboard.setText(text, text);
    }

    @Override
    public void onSaveImageToClipboard(String url) {
        mClipboard.setHTMLText("<img src=\"" + url + "\">", url, url);
    }

    @Override
    public void onOpenInNewTab(String url, Referrer referrer) {
        RecordUserAction.record("MobileNewTabOpened");
        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        loadUrlParams.setReferrer(referrer);
        mActivity.getTabModelSelector().openNewTab(loadUrlParams,
                TabLaunchType.FROM_LONGPRESS_BACKGROUND, mTab, isIncognito());
    }

    @Override
    public void onReloadIgnoringCache() {
        mTab.reloadIgnoringCache();
    }

    @Override
    public void onLoadOriginalImage() {
        mTab.loadOriginalImage();
    }

    @Override
    public void onOpenInNewIncognitoTab(String url) {
        RecordUserAction.record("MobileNewTabOpened");
        mActivity.getTabModelSelector().openNewTab(new LoadUrlParams(url),
                TabLaunchType.FROM_LONGPRESS_FOREGROUND, mTab, true);
    }

    @Override
    public String getPageUrl() {
        return mTab.getUrl();
    }

    @Override
    public void onOpenImageUrl(String url, Referrer referrer) {
        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        loadUrlParams.setTransitionType(PageTransition.LINK);
        loadUrlParams.setReferrer(referrer);
        mTab.loadUrl(loadUrlParams);
    }

    @Override
    public void onOpenImageInNewTab(String url, Referrer referrer) {
        boolean useOriginal = isSpdyProxyEnabledForUrl(url);
        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        loadUrlParams.setVerbatimHeaders(useOriginal ? PAGESPEED_PASSTHROUGH_HEADERS : null);
        loadUrlParams.setReferrer(referrer);
        mActivity.getTabModelSelector().openNewTab(loadUrlParams,
                TabLaunchType.FROM_LONGPRESS_BACKGROUND, mTab, isIncognito());
    }

    /**
     * Checks if spdy proxy is enabled for input url.
     * @param url Input url to check for spdy setting.
     * @return true if url is enabled for spdy proxy.
    */
    boolean isSpdyProxyEnabledForUrl(String url) {
        if (DataReductionProxySettings.getInstance().isDataReductionProxyEnabled()
                && url != null && !url.toLowerCase(Locale.US).startsWith("https://")
                && !isIncognito()) {
            return true;
        }
        return false;
    }

    /**
     * Remember if the last load used the data reduction proxy, and if so,
     * also remember if it used pass through mode.
     */
    private void maybeSetDataReductionProxyUsed() {
        // Ignore internal URLs.
        String url = mTab.getUrl();
        if (url != null && url.toLowerCase(Locale.US).startsWith("chrome://")) {
            return;
        }
        mUsedSpdyProxy = false;
        mUsedSpdyProxyWithPassthrough = false;
        if (isSpdyProxyEnabledForUrl(url)) {
            mUsedSpdyProxy = true;
            if (mLastPageLoadHasSpdyProxyPassthroughHeaders) {
                mLastPageLoadHasSpdyProxyPassthroughHeaders = false;
                mUsedSpdyProxyWithPassthrough = true;
            }
        }
    }
}

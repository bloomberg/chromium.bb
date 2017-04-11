// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.webkit.WebSettings.LayoutAlgorithm;
import android.webkit.WebSettings.PluginState;
import android.webkit.WebSettings.RenderPriority;
import android.webkit.WebSettings.ZoomDensity;

import org.chromium.android_webview.AwSettings;
import org.chromium.base.BuildInfo;
import org.chromium.base.annotations.SuppressFBWarnings;

/**
 * Type adaptation layer between {@link android.webkit.WebSettings} and
 * {@link org.chromium.android_webview.AwSettings}.
 */
@SuppressWarnings("deprecation")
@SuppressFBWarnings("CHROMIUM_SYNCHRONIZED_METHOD")
public class ContentSettingsAdapter extends android.webkit.WebSettings {
    private AwSettings mAwSettings;

    public ContentSettingsAdapter(AwSettings awSettings) {
        mAwSettings = awSettings;
    }

    AwSettings getAwSettings() {
        return mAwSettings;
    }

    @Override
    @Deprecated
    public void setNavDump(boolean enabled) {
        // Intentional no-op.
    }

    @Override
    @Deprecated
    public boolean getNavDump() {
        // Intentional no-op.
        return false;
    }

    @Override
    public void setSupportZoom(boolean support) {
        mAwSettings.setSupportZoom(support);
    }

    @Override
    public boolean supportZoom() {
        return mAwSettings.supportZoom();
    }

    @Override
    public void setBuiltInZoomControls(boolean enabled) {
        mAwSettings.setBuiltInZoomControls(enabled);
    }

    @Override
    public boolean getBuiltInZoomControls() {
        return mAwSettings.getBuiltInZoomControls();
    }

    @Override
    public void setDisplayZoomControls(boolean enabled) {
        mAwSettings.setDisplayZoomControls(enabled);
    }

    @Override
    public boolean getDisplayZoomControls() {
        return mAwSettings.getDisplayZoomControls();
    }

    @Override
    public void setAllowFileAccess(boolean allow) {
        mAwSettings.setAllowFileAccess(allow);
    }

    @Override
    public boolean getAllowFileAccess() {
        return mAwSettings.getAllowFileAccess();
    }

    @Override
    public void setAllowContentAccess(boolean allow) {
        mAwSettings.setAllowContentAccess(allow);
    }

    @Override
    public boolean getAllowContentAccess() {
        return mAwSettings.getAllowContentAccess();
    }

    @Override
    public void setLoadWithOverviewMode(boolean overview) {
        mAwSettings.setLoadWithOverviewMode(overview);
    }

    @Override
    public boolean getLoadWithOverviewMode() {
        return mAwSettings.getLoadWithOverviewMode();
    }

    // TODO(ntfschr): add @Override once Android O is released (crbug/706631)
    public void setSafeBrowsingEnabled(boolean accept) {
        mAwSettings.setSafeBrowsingEnabled(accept);
    }

    // TODO(ntfschr): add @Override once Android O is released (crbug/706631)
    public boolean getSafeBrowsingEnabled() {
        return mAwSettings.getSafeBrowsingEnabled();
    }

    @Override
    public void setAcceptThirdPartyCookies(boolean accept) {
        mAwSettings.setAcceptThirdPartyCookies(accept);
    }

    @Override
    public boolean getAcceptThirdPartyCookies() {
        return mAwSettings.getAcceptThirdPartyCookies();
    }

    @Override
    public void setEnableSmoothTransition(boolean enable) {
        // Intentional no-op.
    }

    @Override
    public boolean enableSmoothTransition() {
        // Intentional no-op.
        return false;
    }

    @Override
    public void setUseWebViewBackgroundForOverscrollBackground(boolean view) {
        // Intentional no-op.
    }

    @Override
    public boolean getUseWebViewBackgroundForOverscrollBackground() {
        // Intentional no-op.
        return false;
    }

    @Override
    public void setSaveFormData(boolean save) {
        if (BuildInfo.isAtLeastO()) return;

        mAwSettings.setSaveFormData(save);
    }

    @Override
    public boolean getSaveFormData() {
        if (BuildInfo.isAtLeastO()) return false;

        return mAwSettings.getSaveFormData();
    }

    @Override
    public void setSavePassword(boolean save) {
        // Intentional no-op.
    }

    @Override
    public boolean getSavePassword() {
        // Intentional no-op.
        return false;
    }

    @Override
    public synchronized void setTextZoom(int textZoom) {
        mAwSettings.setTextZoom(textZoom);
    }

    @Override
    public synchronized int getTextZoom() {
        return mAwSettings.getTextZoom();
    }

    @Override
    public void setDefaultZoom(ZoomDensity zoom) {
        mAwSettings.setDefaultZoom(zoom);
    }

    @Override
    public ZoomDensity getDefaultZoom() {
        return mAwSettings.getDefaultZoom();
    }

    @Override
    public void setLightTouchEnabled(boolean enabled) {
        // Intentional no-op.
    }

    @Override
    public boolean getLightTouchEnabled() {
        // Intentional no-op.
        return false;
    }

    @Override
    public synchronized void setUserAgent(int ua) {
        mAwSettings.setUserAgent(ua);
    }

    @Override
    public synchronized int getUserAgent() {
        // Minimal implementation for backwards compatibility: just identifies default vs custom.
        return AwSettings.getDefaultUserAgent().equals(getUserAgentString()) ? 0 : -1;
    }

    @Override
    public synchronized void setUseWideViewPort(boolean use) {
        mAwSettings.setUseWideViewPort(use);
    }

    @Override
    public synchronized boolean getUseWideViewPort() {
        return mAwSettings.getUseWideViewPort();
    }

    @Override
    public synchronized void setSupportMultipleWindows(boolean support) {
        mAwSettings.setSupportMultipleWindows(support);
    }

    @Override
    public synchronized boolean supportMultipleWindows() {
        return mAwSettings.supportMultipleWindows();
    }

    @Override
    public synchronized void setLayoutAlgorithm(LayoutAlgorithm l) {
        mAwSettings.setLayoutAlgorithm(l);
    }

    @Override
    public synchronized LayoutAlgorithm getLayoutAlgorithm() {
        return mAwSettings.getLayoutAlgorithm();
    }

    @Override
    public synchronized void setStandardFontFamily(String font) {
        mAwSettings.setStandardFontFamily(font);
    }

    @Override
    public synchronized String getStandardFontFamily() {
        return mAwSettings.getStandardFontFamily();
    }

    @Override
    public synchronized void setFixedFontFamily(String font) {
        mAwSettings.setFixedFontFamily(font);
    }

    @Override
    public synchronized String getFixedFontFamily() {
        return mAwSettings.getFixedFontFamily();
    }

    @Override
    public synchronized void setSansSerifFontFamily(String font) {
        mAwSettings.setSansSerifFontFamily(font);
    }

    @Override
    public synchronized String getSansSerifFontFamily() {
        return mAwSettings.getSansSerifFontFamily();
    }

    @Override
    public synchronized void setSerifFontFamily(String font) {
        mAwSettings.setSerifFontFamily(font);
    }

    @Override
    public synchronized String getSerifFontFamily() {
        return mAwSettings.getSerifFontFamily();
    }

    @Override
    public synchronized void setCursiveFontFamily(String font) {
        mAwSettings.setCursiveFontFamily(font);
    }

    @Override
    public synchronized String getCursiveFontFamily() {
        return mAwSettings.getCursiveFontFamily();
    }

    @Override
    public synchronized void setFantasyFontFamily(String font) {
        mAwSettings.setFantasyFontFamily(font);
    }

    @Override
    public synchronized String getFantasyFontFamily() {
        return mAwSettings.getFantasyFontFamily();
    }

    @Override
    public synchronized void setMinimumFontSize(int size) {
        mAwSettings.setMinimumFontSize(size);
    }

    @Override
    public synchronized int getMinimumFontSize() {
        return mAwSettings.getMinimumFontSize();
    }

    @Override
    public synchronized void setMinimumLogicalFontSize(int size) {
        mAwSettings.setMinimumLogicalFontSize(size);
    }

    @Override
    public synchronized int getMinimumLogicalFontSize() {
        return mAwSettings.getMinimumLogicalFontSize();
    }

    @Override
    public synchronized void setDefaultFontSize(int size) {
        mAwSettings.setDefaultFontSize(size);
    }

    @Override
    public synchronized int getDefaultFontSize() {
        return mAwSettings.getDefaultFontSize();
    }

    @Override
    public synchronized void setDefaultFixedFontSize(int size) {
        mAwSettings.setDefaultFixedFontSize(size);
    }

    @Override
    public synchronized int getDefaultFixedFontSize() {
        return mAwSettings.getDefaultFixedFontSize();
    }

    @Override
    public synchronized void setLoadsImagesAutomatically(boolean flag) {
        mAwSettings.setLoadsImagesAutomatically(flag);
    }

    @Override
    public synchronized boolean getLoadsImagesAutomatically() {
        return mAwSettings.getLoadsImagesAutomatically();
    }

    @Override
    public synchronized void setBlockNetworkImage(boolean flag) {
        mAwSettings.setImagesEnabled(!flag);
    }

    @Override
    public synchronized boolean getBlockNetworkImage() {
        return !mAwSettings.getImagesEnabled();
    }

    @Override
    public synchronized void setBlockNetworkLoads(boolean flag) {
        mAwSettings.setBlockNetworkLoads(flag);
    }

    @Override
    public synchronized boolean getBlockNetworkLoads() {
        return mAwSettings.getBlockNetworkLoads();
    }

    @Override
    public synchronized void setJavaScriptEnabled(boolean flag) {
        mAwSettings.setJavaScriptEnabled(flag);
    }

    @Override
    public void setAllowUniversalAccessFromFileURLs(boolean flag) {
        mAwSettings.setAllowUniversalAccessFromFileURLs(flag);
    }

    @Override
    public void setAllowFileAccessFromFileURLs(boolean flag) {
        mAwSettings.setAllowFileAccessFromFileURLs(flag);
    }

    @Override
    public synchronized void setPluginsEnabled(boolean flag) {
        mAwSettings.setPluginsEnabled(flag);
    }

    @Override
    public synchronized void setPluginState(PluginState state) {
        mAwSettings.setPluginState(state);
    }

    @Override
    public synchronized void setDatabasePath(String databasePath) {
        // Intentional no-op.
    }

    @Override
    public synchronized void setGeolocationDatabasePath(String databasePath) {
        // Intentional no-op.
    }

    @Override
    public synchronized void setAppCacheEnabled(boolean flag) {
        mAwSettings.setAppCacheEnabled(flag);
    }

    @Override
    public synchronized void setAppCachePath(String appCachePath) {
        mAwSettings.setAppCachePath(appCachePath);
    }

    @Override
    public synchronized void setAppCacheMaxSize(long appCacheMaxSize) {
        // Intentional no-op.
    }

    @Override
    public synchronized void setDatabaseEnabled(boolean flag) {
        mAwSettings.setDatabaseEnabled(flag);
    }

    @Override
    public synchronized void setDomStorageEnabled(boolean flag) {
        mAwSettings.setDomStorageEnabled(flag);
    }

    @Override
    public synchronized boolean getDomStorageEnabled() {
        return mAwSettings.getDomStorageEnabled();
    }

    @Override
    public synchronized String getDatabasePath() {
        // Intentional no-op.
        return "";
    }

    @Override
    public synchronized boolean getDatabaseEnabled() {
        return mAwSettings.getDatabaseEnabled();
    }

    @Override
    public synchronized void setGeolocationEnabled(boolean flag) {
        mAwSettings.setGeolocationEnabled(flag);
    }

    @Override
    public synchronized boolean getJavaScriptEnabled() {
        return mAwSettings.getJavaScriptEnabled();
    }

    @Override
    public boolean getAllowUniversalAccessFromFileURLs() {
        return mAwSettings.getAllowUniversalAccessFromFileURLs();
    }

    @Override
    public boolean getAllowFileAccessFromFileURLs() {
        return mAwSettings.getAllowFileAccessFromFileURLs();
    }

    @Override
    public synchronized boolean getPluginsEnabled() {
        return mAwSettings.getPluginsEnabled();
    }

    @Override
    public synchronized PluginState getPluginState() {
        return mAwSettings.getPluginState();
    }

    @Override
    public synchronized void setJavaScriptCanOpenWindowsAutomatically(boolean flag) {
        mAwSettings.setJavaScriptCanOpenWindowsAutomatically(flag);
    }

    @Override
    public synchronized boolean getJavaScriptCanOpenWindowsAutomatically() {
        return mAwSettings.getJavaScriptCanOpenWindowsAutomatically();
    }

    @Override
    public synchronized void setDefaultTextEncodingName(String encoding) {
        mAwSettings.setDefaultTextEncodingName(encoding);
    }

    @Override
    public synchronized String getDefaultTextEncodingName() {
        return mAwSettings.getDefaultTextEncodingName();
    }

    @Override
    public synchronized void setUserAgentString(String ua) {
        mAwSettings.setUserAgentString(ua);
    }

    @Override
    public synchronized String getUserAgentString() {
        return mAwSettings.getUserAgentString();
    }

    @Override
    public void setNeedInitialFocus(boolean flag) {
        mAwSettings.setShouldFocusFirstNode(flag);
    }

    @Override
    public synchronized void setRenderPriority(RenderPriority priority) {
        // Intentional no-op.
    }

    @Override
    public void setCacheMode(int mode) {
        mAwSettings.setCacheMode(mode);
    }

    @Override
    public int getCacheMode() {
        return mAwSettings.getCacheMode();
    }

    @Override
    public void setMediaPlaybackRequiresUserGesture(boolean require) {
        mAwSettings.setMediaPlaybackRequiresUserGesture(require);
    }

    @Override
    public boolean getMediaPlaybackRequiresUserGesture() {
        return mAwSettings.getMediaPlaybackRequiresUserGesture();
    }

    @Override
    public void setMixedContentMode(int mode) {
        mAwSettings.setMixedContentMode(mode);
    }

    @Override
    public int getMixedContentMode() {
        return mAwSettings.getMixedContentMode();
    }

    @Override
    public void setOffscreenPreRaster(boolean enabled) {
        mAwSettings.setOffscreenPreRaster(enabled);
    }

    @Override
    public boolean getOffscreenPreRaster() {
        return mAwSettings.getOffscreenPreRaster();
    }

    public void setDisabledActionModeMenuItems(int menuItems) {
        mAwSettings.setDisabledActionModeMenuItems(menuItems);
    }

    public int getDisabledActionModeMenuItems() {
        return mAwSettings.getDisabledActionModeMenuItems();
    }

    @Override
    public void setVideoOverlayForEmbeddedEncryptedVideoEnabled(boolean flag) {
        // No-op, see http://crbug.com/616583
    }

    @Override
    public boolean getVideoOverlayForEmbeddedEncryptedVideoEnabled() {
        // Always false, see http://crbug.com/616583
        return false;
    }
}

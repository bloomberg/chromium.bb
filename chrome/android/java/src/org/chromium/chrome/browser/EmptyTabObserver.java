// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.view.ContextMenu;

/**
 * An implementation of the {@link TabObserver} which has empty implementations of all methods.
 */
public class EmptyTabObserver implements TabObserver {

    @Override
    public void onDestroyed(Tab tab) { }

    @Override
    public void onContentChanged(Tab tab) { }

    @Override
    public void onLoadUrl(Tab tab, String url, int loadType) { }

    @Override
    public void onFaviconUpdated(Tab tab) { }

    @Override
    public void onTitleUpdated(Tab tab) { }

    @Override
    public void onUrlUpdated(Tab tab) { }

    @Override
    public void onSSLStateUpdated(Tab tab) { }

    @Override
    public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) { }

    @Override
    public void onContextMenuShown(Tab tab, ContextMenu menu) { }

    @Override
    public void onLoadStarted(Tab tabBase) { }

    @Override
    public void onLoadStopped(Tab tabBase) { }

    @Override
    public void onLoadProgressChanged(Tab tab, int progress) { }

    @Override
    public void onUpdateUrl(Tab tab, String url) { }

    @Override
    public void onToggleFullscreenMode(Tab tab, boolean enable) { }

    @Override
    public void onDidFailLoad(Tab tab, boolean isProvisionalLoad, boolean isMainFrame,
            int errorCode, String description, String failingUrl) { }

    @Override
    public void onDidStartProvisionalLoadForFrame(Tab tab, long frameId, long parentFrameId,
            boolean isMainFrame, String validatedUrl, boolean isErrorPage,
            boolean isIframeSrcdoc) { }

    @Override
    public void onDidChangeThemeColor(int color) { }
}

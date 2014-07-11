// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.view.ContextMenu;

import org.chromium.content.browser.ContentViewCore;

/**
 * An observer that is notified of changes to a {@link Tab} object.
 */
public interface TabObserver {

    /**
     * Called when a {@link Tab} is being destroyed.
     * @param tab The notifying {@link Tab}.
     */
    void onDestroyed(Tab tab);

    /**
     * Called when the tab content changes (to/from native pages or swapping native WebContents).
     * @param tab The notifying {@link Tab}.
     */
    void onContentChanged(Tab tab);

    /**
     * Called when loadUrl is triggered on a a {@link Tab}.
     * @param tab      The notifying {@link Tab}.
     * @param url      The url that is being loaded.
     * @param loadType The type of load that was performed.
     *
     * @see TabLoadStatus#PAGE_LOAD_FAILED
     * @see TabLoadStatus#DEFAULT_PAGE_LOAD
     * @see TabLoadStatus#PARTIAL_PRERENDERED_PAGE_LOAD
     * @see TabLoadStatus#FULL_PRERENDERED_PAGE_LOAD
     */
    void onLoadUrl(Tab tab, String url, int loadType);

    /**
     * Called when the favicon of a {@link Tab} has been updated.
     * @param tab The notifying {@link Tab}.
     */
    void onFaviconUpdated(Tab tab);

    /**
     * Called when the title of a {@link Tab} changes.
     * @param tab The notifying {@link Tab}.
     */
    void onTitleUpdated(Tab tab);

    /**
     * Called when the URL of a {@link Tab} changes.
     * @param tab The notifying {@link Tab}.
     */
    void onUrlUpdated(Tab tab);

    /**
     * Called when the SSL state of a {@link Tab} changes.
     * @param tab The notifying {@link Tab}.
     */
    void onSSLStateUpdated(Tab tab);

    /**
     * Called when the WebContents of a {@link Tab} have been swapped.
     * @param tab The notifying {@link Tab}.
     * @param didStartLoad Whether WebContentsObserver::DidStartProvisionalLoadForFrame() has
     *     already been called.
     * @param didFinishLoad Whether WebContentsObserver::DidFinishLoad() has already been called.
     */
    void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad);

    /**
     * Called when a context menu is shown for a {@link ContentViewCore} owned by a {@link Tab}.
     * @param tab  The notifying {@link Tab}.
     * @param menu The {@link ContextMenu} that is being shown.
     */
    void onContextMenuShown(Tab tab, ContextMenu menu);

    // WebContentsDelegateAndroid methods ---------------------------------------------------------

    /**
     * Called when the contents loading starts.
     * @param tab The notifying {@link Tab}.
     */
    void onLoadStarted(Tab tab);

    /**
     * Called when the contents loading stops.
     * @param tab The notifying {@link Tab}.
     */
    void onLoadStopped(Tab tab);

    /**
     * Called when the load progress of a {@link Tab} changes.
     * @param tab      The notifying {@link Tab}.
     * @param progress The new progress from [0,100].
     */
    void onLoadProgressChanged(Tab tab, int progress);

    /**
     * Called when the URL of a {@link Tab} changes.
     * @param tab The notifying {@link Tab}.
     * @param url The new URL.
     */
    void onUpdateUrl(Tab tab, String url);

    /**
     * Called when the {@link Tab} should enter or leave fullscreen mode.
     * @param tab    The notifying {@link Tab}.
     * @param enable Whether or not to enter fullscreen mode.
     */
    void onToggleFullscreenMode(Tab tab, boolean enable);

    // WebContentsObserverAndroid methods ---------------------------------------------------------

    /**
     * Called when an error occurs while loading a page and/or the page fails to load.
     * @param tab               The notifying {@link Tab}.
     * @param isProvisionalLoad Whether the failed load occurred during the provisional load.
     * @param isMainFrame       Whether failed load happened for the main frame.
     * @param errorCode         Code for the occurring error.
     * @param description       The description for the error.
     * @param failingUrl        The url that was loading when the error occurred.
     */
    void onDidFailLoad(
            Tab tab, boolean isProvisionalLoad, boolean isMainFrame, int errorCode,
            String description, String failingUrl);

    /**
     * Called when load is started for a given frame.
     * @param tab            The notifying {@link Tab}.
     * @param frameId        A positive, non-zero integer identifying the navigating frame.
     * @param parentFrameId  The frame identifier of the frame containing the navigating frame,
     *                       or -1 if the frame is not contained in another frame.
     * @param isMainFrame    Whether the load is happening for the main frame.
     * @param validatedUrl   The validated URL that is being navigated to.
     * @param isErrorPage    Whether this is navigating to an error page.
     * @param isIframeSrcdoc Whether this is navigating to about:srcdoc.
     */
    public void onDidStartProvisionalLoadForFrame(
            Tab tab, long frameId, long parentFrameId, boolean isMainFrame, String validatedUrl,
            boolean isErrorPage, boolean isIframeSrcdoc);

    /**
     * Called when the theme color is changed
     * @param color the new color in ARGB format.
     */
    public void onDidChangeThemeColor(int color);
}

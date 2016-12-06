// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import java.lang.ref.WeakReference;

/**
 * This class receives callbacks that act as hooks for various a native web contents events related
 * to loading a url. A single web contents can have multiple WebContentObservers.
 */
public abstract class WebContentsObserver {
    // TODO(jdduke): Remove the destroy method and hold observer embedders
    // responsible for explicit observer detachment.
    // Using a weak reference avoids cycles that might prevent GC of WebView's WebContents.
    private WeakReference<WebContents> mWebContents;

    public WebContentsObserver(WebContents webContents) {
        mWebContents = new WeakReference<WebContents>(webContents);
        webContents.addObserver(this);
    }

    /**
     * Called when the RenderView of the current RenderViewHost is ready, e.g. because we recreated
     * it after a crash.
     */
    public void renderViewReady() {}

    public void renderProcessGone(boolean wasOomProtected) {}

    /**
     * Called when the current navigation finishes.
     *
     * @param isMainFrame Whether the navigation is for the main frame.
     * @param isErrorPage Whether the navigation shows an error page.
     * @param hasCommitted Whether the navigation has committed. This returns true for either
     *                     successful commits or error pages that replace the previous page
     *                     (distinguished by |isErrorPage|), and false for errors that leave the
     *                     user on the previous page.
     */
    public void didFinishNavigation(
            boolean isMainFrame, boolean isErrorPage, boolean hasCommitted) {}

    /**
     * Called when the a page starts loading.
     * @param url The validated url for the loading page.
     */
    public void didStartLoading(String url) {}

    /**
     * Called when the a page finishes loading.
     * @param url The validated url for the page.
     */
    public void didStopLoading(String url) {}

    /**
     * Called when an error occurs while loading a page and/or the page fails to load.
     * @param errorCode Error code for the occurring error.
     * @param description The description for the error.
     * @param failingUrl The url that was loading when the error occurred.
     */
    public void didFailLoad(boolean isProvisionalLoad, boolean isMainFrame, int errorCode,
            String description, String failingUrl, boolean wasIgnoredByHandler) {}

    /**
     * Called when the main frame of the page has committed.
     * @param url The validated url for the page.
     * @param baseUrl The validated base url for the page.
     * @param isNavigationToDifferentPage Whether the main frame navigated to a different page.
     * @param isFragmentNavigation Whether the main frame navigation did not cause changes to the
     *                             document (for example scrolling to a named anchor or PopState).
     * @param statusCode The HTTP status code of the navigation.
     */
    public void didNavigateMainFrame(String url, String baseUrl,
            boolean isNavigationToDifferentPage, boolean isFragmentNavigation, int statusCode) {}

    /**
     * Called when the page had painted something non-empty.
     */
    public void didFirstVisuallyNonEmptyPaint() {}

    /**
     * The web contents was shown.
     */
    public void wasShown() {}

    /**
     * The web contents was hidden.
     */
    public void wasHidden() {}

    /**
     * Title was set.
     * @param title The updated title.
     */
    public void titleWasSet(String title) {}

    /**
     * Similar to didNavigateMainFrame but also called on subframe navigations.
     * @param url The validated url for the page.
     * @param baseUrl The validated base url for the page.
     * @param isReload True if this navigation is a reload.
     */
    public void didNavigateAnyFrame(String url, String baseUrl, boolean isReload) {}

    /**
     * Called once the window.document object of the main frame was created.
     */
    public void documentAvailableInMainFrame() {}

    /**
     * Notifies that a load is started for a given frame.
     * @param frameId A positive, non-zero integer identifying the navigating frame.
     * @param parentFrameId The frame identifier of the frame containing the navigating frame,
     *                      or -1 if the frame is not contained in another frame.
     * @param isMainFrame Whether the load is happening for the main frame.
     * @param validatedUrl The validated URL that is being navigated to.
     * @param isErrorPage Whether this is navigating to an error page.
     */
    public void didStartProvisionalLoadForFrame(long frameId, long parentFrameId,
            boolean isMainFrame, String validatedUrl, boolean isErrorPage) {
    }

    /**
     * Notifies that the provisional load was successfully committed. The RenderViewHost is now
     * the current RenderViewHost of the WebContents.
     * @param frameId A positive, non-zero integer identifying the navigating frame.
     * @param isMainFrame Whether the load is happening for the main frame.
     * @param url The committed URL being navigated to.
     * @param transitionType The transition type as defined in
     *                      {@link org.chromium.ui.base.PageTransition} for the load.
     */
    public void didCommitProvisionalLoadForFrame(
            long frameId, boolean isMainFrame, String url, int transitionType) {}

    /**
     * Notifies that a load has finished for a given frame.
     * @param frameId A positive, non-zero integer identifying the navigating frame.
     * @param validatedUrl The validated URL that is being navigated to.
     * @param isMainFrame Whether the load is happening for the main frame.
     */
    public void didFinishLoad(long frameId, String validatedUrl, boolean isMainFrame) {}

    /**
     * Notifies that the document has finished loading for the given frame.
     * @param frameId A positive, non-zero integer identifying the navigating frame.
     */
    public void documentLoadedInFrame(long frameId, boolean isMainFrame) {}

    /**
     * Notifies that a navigation entry has been committed.
     */
    public void navigationEntryCommitted() {}

    /**
     * Called when an interstitial page gets attached to the tab content.
     */
    public void didAttachInterstitialPage() {}

    /**
     * Called when an interstitial page gets detached from the tab content.
     */
    public void didDetachInterstitialPage() {}

    /**
     * Called when the theme color was changed.
     * @param color the new color in ARGB format
     */
    public void didChangeThemeColor(int color) {}

    /**
     * Called when we started navigation to the pending entry.
     * @param url        The URL that we are navigating to.
     */
    public void didStartNavigationToPendingEntry(String url) {}

    /**
     * Stop observing the web contents and clean up associated references.
     */
    public void destroy() {
        if (mWebContents == null) return;
        final WebContents webContents = mWebContents.get();
        mWebContents = null;
        if (webContents == null) return;
        webContents.removeObserver(this);
    }

    protected WebContentsObserver() {}
}

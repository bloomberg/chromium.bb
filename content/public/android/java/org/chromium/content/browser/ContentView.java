// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.widget.FrameLayout;

import org.chromium.content.browser.AndroidBrowserProcess;

public class ContentView extends FrameLayout {

    /**
     * Automatically decide the number of renderer processes to use based on device memory class.
     * */
    public static final int MAX_RENDERERS_AUTOMATIC = -1;

    /**
     * Enable multi-process ContentView. This should be called by the application before
     * constructing any ContentView instances. If enabled, ContentView will run renderers in
     * separate processes up to the number of processes specified by maxRenderProcesses. If this is
     * not called then the default is to run the renderer in the main application on a separate
     * thread.
     *
     * @param context Context used to obtain the application context.
     * @param maxRendererProcesses Limit on the number of renderers to use. Each tab runs in its own
     * process until the maximum number of processes is reached. The special value of
     * MAX_RENDERERS_SINGLE_PROCESS requests single-process mode where the renderer will run in the
     * application process in a separate thread. If the special value MAX_RENDERERS_AUTOMATIC is
     * used then the number of renderers will be determined based on the device memory class. The
     * maximum number of allowed renderers is capped by MAX_RENDERERS_LIMIT.
     */
    public static void enableMultiProcess(Context context, int maxRendererProcesses) {
        AndroidBrowserProcess.initContentViewProcess(context, maxRendererProcesses);
    }

    /**
     * Registers the drawable to be used for overlaying the popup zoomer contents.  The drawable
     * should be transparent in the middle to allow the zoomed content to show.
     *
     * @param id The id of the drawable to be used to overlay the popup zoomer contents.
     */
    public static void registerPopupOverlayResourceId(int id) {
        // TODO(tedchoc): Implement.
    }

    /**
     * Sets how much to round the corners of the popup contents.
     * @param r The radius of the rounded corners of the popup overlay drawable.
     */
    public static void registerPopupOverlayCornerRadius(float r) {
        // TODO(tedchoc): Implement.
    }

    public ContentView(Context context) {
        super(context, null);
    }

    /**
     * Load url without fixing up the url string. Calls from Chrome should be not
     * be using this, but should use Tab.loadUrl instead.
     * @param url The url to load.
     */
    public void loadUrlWithoutUrlSanitization(String url) {
        // TODO(tedchoc): Implement.
    }

    /**
     * Get the URL of the current page.
     *
     * @return The URL of the current page.
     */
    public String getUrl() {
        // TODO(tedchoc): Implement.
        return null;
    }

    /**
     * @return Whether the current WebContents has a previous navigation entry.
     */
    public boolean canGoBack() {
        // TODO(tedchoc): Implement.
        return false;
    }

    /**
     * @return Whether the current WebContents has a navigation entry after the current one.
     */
    public boolean canGoForward() {
        // TODO(tedchoc): Implement.
        return false;
    }

    /**
     * Goes to the navigation entry before the current one.
     */
    public void goBack() {
        // TODO(tedchoc): Implement.
    }

    /**
     * Goes to the navigation entry following the current one.
     */
    public void goForward() {
        // TODO(tedchoc): Implement.
    }

    /**
     * Reload the current page.
     */
    public void reload() {
        // TODO(tedchoc): Implement.
    }
}

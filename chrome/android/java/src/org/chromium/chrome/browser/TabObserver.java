// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.view.ContextMenu;

import org.chromium.content.browser.ContentView;

/**
 * An observer that is notified of changes to a {@link TabBase} object.
 */
public interface TabObserver {

    /**
     * Called when a {@link TabBase} is being destroyed.
     * @param tab The notifying {@link TabBase}.
     */
    void onDestroyed(TabBase tab);

    /**
     * Called when the tab content changes (to/from native pages or swapping native WebContents).
     * @param tab The notifying {@link TabBase}.
     */
    void onContentChanged(TabBase tab);

    /**
     * Called when the favicon of a {@link TabBase} has been updated.
     * @param tab The notifying {@link TabBase}.
     */
    void onFaviconUpdated(TabBase tab);

    /**
     * Called when the WebContents of a {@link TabBase} have been swapped.
     * @param tab The notifying {@link TabBase}.
     */
    void onWebContentsSwapped(TabBase tab);

    /**
     * Called when a context menu is shown for a {@link ContentView} owned by a {@link TabBase}.
     * @param tab  The notifying {@link TabBase}.
     * @param menu The {@link ContextMenu} that is being shown.
     */
    void onContextMenuShown(TabBase tab, ContextMenu menu);

    // WebContentsDelegateAndroid methods ---------------------------------------------------------

    /**
     * Called when the load progress of a {@link TabBase} changes.
     * @param tab      The notifying {@link TabBase}.
     * @param progress The new progress from [0,100].
     */
    void onLoadProgressChanged(TabBase tab, int progress);

    /**
     * Called when the URL of a {@link TabBase} changes.
     * @param tab The notifying {@link TabBase}.
     * @param url The new URL.
     */
    void onUpdateUrl(TabBase tab, String url);

    /**
     * Called when the {@link TabBase} should enter or leave fullscreen mode.
     * @param tab    The notifying {@link TabBase}.
     * @param enable Whether or not to enter fullscreen mode.
     */
    void onToggleFullscreenMode(TabBase tab, boolean enable);

    // WebContentsObserverAndroid methods ---------------------------------------------------------

    /**
     * Called when an error occurs while loading a page and/or the page fails to load.
     * @param tab               The notifying {@link TabBase}.
     * @param isProvisionalLoad Whether the failed load occurred during the provisional load.
     * @param isMainFrame       Whether failed load happened for the main frame.
     * @param errorCode         Code for the occurring error.
     * @param description       The description for the error.
     * @param failingUrl        The url that was loading when the error occurred.
     */
    void onDidFailLoad(TabBase tab, boolean isProvisionalLoad, boolean isMainFrame, int errorCode,
            String description, String failingUrl);
}

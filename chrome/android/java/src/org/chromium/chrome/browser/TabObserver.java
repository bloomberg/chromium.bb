// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

/**
 * An observer that is notified of changes to a {@link TabBase} object.
 */
public interface TabObserver {

    /**
     * Called when the load progress of a {@link TabBase} changes.
     * @param tab      The notifying {@link TabBase}.
     * @param progress The new progress from [0,100].
     */
    public void onLoadProgressChanged(TabBase tab, int progress);

    /**
     * Called when the URL of a {@link TabBase} changes.
     * @param tab The notifying {@link TabBase}.
     * @param url The new URL.
     */
    public void onUpdateUrl(TabBase tab, String url);

    /**
     * Called when a {@link TabBase} is being destroyed.
     * @param tab The notifying {@link TabBase}.
     */
    public void onDestroyed(TabBase tab);

    /**
     * Called when the tab content changes (to/from native pages or swapping native WebContents).
     * @param tab The notifying {@link TabBase}.
     */
    public void onContentChanged(TabBase tab);
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

/**
 * Observes changes to the {@link TabBase} class.  A {@link TabObserver} can observe multiple
 * {@link TabBase} objects, as the first parameter for each method is the specific {@link TabBase}
 * that is affected.  Additionally, a {@link TabBase} can have multiple {@link TabObserver}s.
 *
 * Note that this interface does not expose control methods, only observation methods.
 */
public interface TabObserver {
    /**
     * Called when the load progress of a {@link TabBase} has changed.
     * @param tab      The notifying {@link TabBase}.
     * @param progress The new progress amount (from 0 to 100).
     */
    public void onLoadProgressChanged(TabBase tab, int progress);

    /**
     * Called when the URL of a {@link TabBase} has changed.
     * @param tab The notifying {@link TabBase}.
     * @param url The new URL.
     */
    public void onUpdateUrl(TabBase tab, String url);

    /**
     * Called when the tab is about to close.
     * @param tab The closing {@link TabBase}.
     */
    public void onCloseTab(TabBase tab);
}
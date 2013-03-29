// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.testshell;

/**
 * Observes changes to the {@link TestShellTab} class.  A {@link TestShellTabObserver} can observe
 * multiple {@link TestShellTab} objects, as the first parameter for each method is the specific
 * {@link TestShellTab} that is affected.  Additionally, a {@link TestShellTab} can have multiple
 * {@link TestShellTabObserver}s.
 *
 * Note that this interface does not expose control methods, only observation methods.
 */
public interface TestShellTabObserver {
    /**
     * Called when the load progress of a {@link TestShellTab} has changed.
     * @param tab      The notifying {@link TestShellTab}.
     * @param progress The new progress amount (from 0 to 100).
     */
    public void onLoadProgressChanged(TestShellTab tab, int progress);

    /**
     * Called when the URL of a {@link TestShellTab} has changed.
     * @param tab The notifying {@link TestShellTab}.
     * @param url The new URL.
     */
    public void onUpdateUrl(TestShellTab tab, String url);

    /**
     * Called when the tab is about to close.
     * @param tab The closing {@link TestShellTab}.
     */
    public void onCloseTab(TestShellTab tab);
}
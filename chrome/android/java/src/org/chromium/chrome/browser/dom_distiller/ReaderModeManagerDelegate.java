// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.bottombar.readermode.ReaderModePanel;
import org.chromium.content_public.browser.WebContents;

/**
 * Delegate for the panel to call into the manager.
 */
public interface ReaderModeManagerDelegate {
    /**
     * @return Reader mode header background color.
     */
    int getReaderModeHeaderBackgroundColor();

    /**
     * @return One of ReaderModeManager.POSSIBLE, NOT_POSSIBLE, STARTED constants.
     */
    int getReaderModeStatus();

    /**
     * @param panel The panel to be managed.
     */
    void setReaderModePanel(ReaderModePanel panel);

    /**
     * Load a URL in a new tab.
     * @param url The URL to load in the tab.
     */
    void createNewTab(String url);

    /**
     * Notify the manager that the panel was closed using the "x" icon.
     */
    void onCloseButtonPressed();

    /**
     * Get the WebContents of the page that is being distilled.
     * @return The WebContents for the currently visible tab.
     */
    WebContents getBasePageWebContents();

    /**
     * Close the Reader Mode panel. This method wrap's the ReaderModePanel's close function and
     * checks for null.
     * @param reason The reason the panel is being closed.
     * @param animate If the panel should animate as it closes.
     */
    void closeReaderPanel(StateChangeReason reason, boolean animate);

    /**
     * @return The ChromeActivity that owns the manager.
     */
    ChromeActivity getChromeActivity();

    /**
     * Record the amount of time that a user spent in the Reader Mode panel.
     * @param timeInMs The amount of time spent in ms.
     */
    void recordTimeSpentInReader(long timeInMs);

    /**
     * Notification that the size of the view has changed.
     */
    void onSizeChanged();
}

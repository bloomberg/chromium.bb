// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel.PanelState;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel.StateChangeReason;

/**
 * The delegate that that interfaces with the {@link ContextualSearchPanel}.
 */
public interface ContextualSearchPanelDelegate {
    /**
     * @return Whether the Panel is showing.
     */
    boolean isShowing();

    /**
     * @return Whether the Search Bar is peeking.
     */
    boolean isPeeking();

    /**
     * Maximizes the Contextual Search Panel, then promotes it to a regular Tab.
     * @param reason The {@code StateChangeReason} behind the maximization and promotion to tab.
     */
    void maximizePanelThenPromoteToTab(StateChangeReason reason);

    /**
     * Maximizes the Contextual Search Panel, then promotes it to a regular Tab.
     * @param reason The {@code StateChangeReason} behind the maximization and promotion to tab.
     * @param duration The animation duration in milliseconds.
     */
    void maximizePanelThenPromoteToTab(StateChangeReason reason, long duration);

    /**
     * Animates the Contextual Search Panel to its peeked state.
     * @param reason The {@code StateChangeReason} to which closing the panel is attributed.
     */
    void peekPanel(StateChangeReason reason);

    /**
     * Animates the Contextual Search Panel to its closed state.
     * @param reason The {@code StateChangeReason} to which closing the panel is attributed.
     * @param animate Whether to animate the close action.
     */
    void closePanel(StateChangeReason reason, boolean animate);

    /**
     * Updates the y coordinate of the Base Page's selection start position.
     * @param y The y coordinate.
     */
    void updateBasePageSelectionYPx(float y);

    /**
     * Sets the content height of the First Run Flow Panel.
     * @param height The content height of the First Run Flow Panel.
     */
    void setPromoContentHeight(float height);

    /**
     * @param shouldHidePromoHeader Sets whether the First Run Flow's header
     *            should be hidden.
     */
    void setShouldHidePromoHeader(boolean shouldHidePromoHeader);

    /**
     * Animates the Contextual Search panel after first-run success.
     */
    void animateAfterFirstRunSuccess();

    /**
     * Handles the onLoadStarted event in the WebContents.
     */
    void onLoadStarted();

    /**
     * Handles the onLoadStopped event in the WebContents.
     */
    void onLoadStopped();

    /**
     * Handles the onLoadProgressChanged event in the WebContents.
     * @param progress The loading progress in percentage (from 0 to 100).
     */
    void onLoadProgressChanged(int progress);

    /**
     * @return The panel's state.
     */
    PanelState getPanelState();

    /**
     * Sets that the contextual search involved the promo.
     */
    void setDidSearchInvolvePromo();

    /**
     * Sets that the Search Content View was seen.
     */
    void setWasSearchContentViewSeen();

    /**
     * Sets whether the promo is active.
     * @param shown Whether the promo is active.
     */
    void setIsPromoActive(boolean shown);

    /**
     * Gets whether a touch on the search content view has been done yet or not.
     */
    boolean didTouchSearchContentView();
}

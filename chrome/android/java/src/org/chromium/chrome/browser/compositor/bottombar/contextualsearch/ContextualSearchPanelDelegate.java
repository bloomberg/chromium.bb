// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContent;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel.PanelState;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel.StateChangeReason;
import org.chromium.content.browser.ContentViewCore;

/**
 * The delegate that that interfaces with the {@link ContextualSearchPanel}.
 */
public interface ContextualSearchPanelDelegate {
    /**
     * @return Whether the Panel is in fullscreen size.
     */
    boolean isFullscreenSizePanel();

    /**
     * @return Whether the Panel is showing.
     */
    boolean isShowing();

    /**
     * @return Whether the Search Bar is peeking.
     */
    boolean isPeeking();

    /**
     * @return The width of the Contextual Search Panel in pixels.
     */
    int getMaximumWidthPx();

    /**
     * @return The height of the Contextual Search Panel in pixels.
     */
    int getMaximumHeightPx();

    /**
     * @return The width of the Search Content View in pixels.
     */
    int getSearchContentViewWidthPx();

    /**
     * @return The height of the Search Content View in pixels.
     */
    int getSearchContentViewHeightPx();

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
     * @return The panel's state.
     */
    PanelState getPanelState();

    /**
     * Sets that the contextual search involved the promo.
     */
    void setDidSearchInvolvePromo();

    /**
     * Sets whether the promo is active.
     * @param shown Whether the promo is active.
     */
    void setIsPromoActive(boolean shown);

    /**
     * Gets whether a touch on the search content view has been done yet or not.
     */
    boolean didTouchSearchContentView();

    /**
     * Called when the SERP finishes loading, this records the duration of loading the SERP from
     * the time the panel was opened until the present.
     * @param wasPrefetch Whether the request was prefetch-enabled.
     */
    void onSearchResultsLoaded(boolean wasPrefetch);

    /**
     * @return {@code true} Whether the close animation should run when the the panel is closed
     *                      due the panel being promoted to a tab.
     */
    boolean shouldAnimatePanelCloseOnPromoteToTab();

    /**
     * @return The ContentViewCore associated with the panel.
     * TODO(mdjones): Remove this method from the interface.
     */
    ContentViewCore getContentViewCore();

    /**
     * Remove the last entry from history provided it is in a given time frame.
     * @param historyUrl The URL to remove.
     * @param urlTimeMs The time that the URL was visited.
     */
    void removeLastHistoryEntry(String historyUrl, long urlTimeMs);

    /**
     * Shows the search term in the BottomBar. This should be called when the search term is set
     * without resolving a search context.
     * @param searchTerm The string that represents the search term.
     */
    void displaySearchTerm(String searchTerm);

    /**
     * Shows the search context in the BottomBar.
     * @param selection The portion of the context that represents the user's selection.
     * @param start The portion of the context from its start to the selection.
     * @param end The portion of the context the selection to its end.
     */
    void displaySearchContext(String selection, String start, String end);

    /**
     * Handles showing the resolved search term in the BottomBarTextControl.
     * @param searchTerm The string that represents the search term.
     */
    void onSearchTermResolutionResponse(String searchTerm);

    /**
     * @param activity The current active ChromeActivity.
     */
    void setChromeActivity(ChromeActivity activity);

    /**
     * Load a URL in the panel ContentViewCore.
     * @param url The URL to load.
     */
    void loadUrlInPanel(String url);

    /**
     * @return True if the ContentViewCore is being shown.
     */
    boolean isContentViewShowing();

    /**
     * @return True if the panel loaded a URL.
     */
    boolean didLoadAnyUrl();

    /**
     * Sets the top control state based on the internals of the panel.
     */
    void updateTopControlState();

    /**
     * Notify the panel that the ContentViewCore was seen.
     */
    void setWasSearchContentViewSeen();

    /**
     * Sets the visibility of the Search Content View.
     * @param isVisible True to make it visible.
     */
    void setSearchContentViewVisibility(boolean isVisible);

    /**
     * @param panelContent The OverlayPanelContent that this panel should use.
     */
    @VisibleForTesting
    void setOverlayPanelContent(OverlayPanelContent panelContent);
}

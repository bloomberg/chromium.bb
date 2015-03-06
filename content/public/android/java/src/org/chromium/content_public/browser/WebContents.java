// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.base.VisibleForTesting;

/**
 * The WebContents Java wrapper to allow communicating with the native WebContents object.
 */
public interface WebContents {
    /**
     * Deletes the Web Contents object.
     */
    void destroy();

    /**
     * @return The navigation controller associated with this WebContents.
     */
    NavigationController getNavigationController();

    /**
     * @return The title for the current visible page.
     */
    String getTitle();

    /**
     * @return The URL for the current visible page.
     */
    String getVisibleUrl();

    /**
     * @return Whether this WebContents is loading a resource.
     */
    boolean isLoading();

    /**
     * @return Whether this WebContents is loading and and the load is to a different top-level
     *         document (rather than being a navigation within the same document).
     */
    boolean isLoadingToDifferentDocument();

    /**
     * Stop any pending navigation.
     */
    void stop();

    /**
     * Inserts css into main frame's document.
     */
    void insertCSS(String css);

    /**
     * To be called when the ContentView is hidden.
     */
    public void onHide();

    /**
     * To be called when the ContentView is shown.
     */
    public void onShow();

    /**
     * Stops all media players for this WebContents.
     */
    public void releaseMediaPlayers();

    /**
     * Get the Background color from underlying RenderWidgetHost for this WebContent.
     */
    public int getBackgroundColor();

    /**
     * Requests the renderer insert a link to the specified stylesheet in the
     * main frame's document.
     */
    void addStyleSheetByURL(String url);

    /**
     * Shows an interstitial page driven by the passed in delegate.
     *
     * @param url The URL being blocked by the interstitial.
     * @param delegate The delegate handling the interstitial.
     */
    @VisibleForTesting
    public void showInterstitialPage(
            String url, long interstitialPageDelegateAndroid);

    /**
     * @return Whether the page is currently showing an interstitial, such as a bad HTTPS page.
     */
    public boolean isShowingInterstitialPage();

    /**
     * If the view is ready to draw contents to the screen. In hardware mode,
     * the initialization of the surface texture may not occur until after the
     * view has been added to the layout. This method will return {@code true}
     * once the texture is actually ready.
     */
    public boolean isReady();

     /**
     * Inform WebKit that Fullscreen mode has been exited by the user.
     */
    public void exitFullscreen();

    /**
     * Changes whether hiding the top controls is enabled.
     *
     * @param enableHiding Whether hiding the top controls should be enabled or not.
     * @param enableShowing Whether showing the top controls should be enabled or not.
     * @param animate Whether the transition should be animated or not.
     */
    public void updateTopControlsState(boolean enableHiding, boolean enableShowing,
            boolean animate);

    /**
     * Shows the IME if the focused widget could accept text input.
     */
    public void showImeIfNeeded();

    /**
     * Brings the Editable to the visible area while IME is up to make easier for inputing text.
     */
    public void scrollFocusedEditableNodeIntoView();

    /**
     * Selects the word around the caret, if any.
     * The caller can check if selection actually occurred by listening to OnSelectionChanged.
     */
    public void selectWordAroundCaret();

    /**
     * Get the URL of the current page.
     *
     * @return The URL of the current page.
     */
    public String getUrl();

    /**
     * Gets the last committed URL. It represents the current page that is
     * displayed in this WebContents. It represents the current security context.
     *
     * @return The last committed URL.
     */
    public String getLastCommittedUrl();

    /**
     * Get the InCognito state of WebContents.
     *
     * @return whether this WebContents is in InCognito mode or not
     */
    public boolean isIncognito();

    /**
     * Resumes the response which is deferred during start.
     */
    public void resumeResponseDeferredAtStart();

    /**
     * Set pending Navigation for transition testing on this WebContents.
     */
    public void setHasPendingNavigationTransitionForTesting();

    /**
     * Set delegate for notifying navigation transition.
     */
    public void setNavigationTransitionDelegate(NavigationTransitionDelegate delegate);

    /**
     * Inserts the provided markup sandboxed into the frame.
     */
    public void setupTransitionView(String markup);

    /**
     * Hides transition elements specified by the selector, and activates any
     * exiting-transition stylesheets.
     */
    public void beginExitTransition(String cssSelector, boolean exitToNativeApp);

    /**
     * Revert the effect of exit transition after it transitions to activity.
     */
    public void revertExitTransition();

    /**
     * Hide transition elements.
     */
    public void hideTransitionElements(String cssSelector);

    /**
     * Show transition elements.
     */
    public void showTransitionElements(String cssSelector);

    /**
     * Clear the navigation transition data.
     */
    public void clearNavigationTransitionData();

    /**
     * Fetch transition elements.
     */
    public void fetchTransitionElements(String url);

    /**
     * Injects the passed Javascript code in the current page and evaluates it.
     * If a result is required, pass in a callback.
     *
     * @param script The Javascript to execute.
     * @param callback The callback to be fired off when a result is ready. The script's
     *                 result will be json encoded and passed as the parameter, and the call
     *                 will be made on the main thread.
     *                 If no result is required, pass null.
     */
    public void evaluateJavaScript(String script, JavaScriptCallback callback);

    /**
     * Adds a log message to dev tools console. |level| must be a value of
     * org.chromium.content_public.common.ConsoleMessageLevel.
     */
    public void addMessageToDevToolsConsole(int level, String message);

    /**
     * Returns whether the initial empty page has been accessed by a script from another
     * page. Always false after the first commit.
     *
     * @return Whether the initial empty page has been accessed by a script.
     */
    public boolean hasAccessedInitialDocument();

    /**
     * This returns the theme color as set by the theme-color meta tag after getting rid of the
     * alpha.
     * @param The default color to be returned if the cached color is not valid.
     * @return The theme color for the content as set by the theme-color meta tag.
     */
    public int getThemeColor(int defaultColor);

    /**
     * Add an observer to the WebContents
     *
     * @param observer The observer to add.
     */
    void addObserver(WebContentsObserver observer);

    /**
     * Remove an observer from the WebContents
     *
     * @param observer The observer to remove.
     */
    void removeObserver(WebContentsObserver observer);
}

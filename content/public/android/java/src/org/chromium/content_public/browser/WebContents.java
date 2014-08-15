// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

/**
 * The WebContents Java wrapper to allow communicating with the native WebContents object.
 */
public interface WebContents {
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
    public void beginExitTransition(String cssSelector);

    /**
     * Injects the passed Javascript code in the current page and evaluates it.
     * If a result is required, pass in a callback.
     *
     * @param script The Javascript to execute.
     * @param callback The callback to be fired off when a result is ready. The script's
     *                 result will be json encoded and passed as the parameter, and the call
     *                 will be made on the main thread.
     *                 If no result is required, pass null.
     * @param startRenderer Tells whether to start Renderer or not for initial empty document
     */
    public void evaluateJavaScript(String script, JavaScriptCallback callback,
            boolean startRenderer);

}

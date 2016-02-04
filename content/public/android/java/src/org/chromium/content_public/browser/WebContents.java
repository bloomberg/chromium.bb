// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.Parcelable;

import org.chromium.base.ObserverList;
import org.chromium.base.VisibleForTesting;

/**
 * The WebContents Java wrapper to allow communicating with the native WebContents object.
 *
 * Note about serialization and {@link Parcelable}:
 *   This object is serializable and deserializable as long as it is done in the same process.  That
 * means it can be passed between Activities inside this process, but not preserved beyond the
 * process lifetime.  This class will automatically deserialize into {@code null} if a deserialize
 * attempt happens in another process.
 *
 * To properly deserialize a custom Parcelable the right class loader must be used.  See below for
 * some examples.
 *
 * Intent Serialization/Deserialization Example:
 * intent.putExtra("WEBCONTENTSKEY", webContents);
 * // ... send to other location ...
 * intent.setExtrasClassLoader(WebContents.class.getClassLoader());
 * webContents = intent.getParcelableExtra("WEBCONTENTSKEY");
 *
 * Bundle Serialization/Deserialization Example:
 * bundle.putParcelable("WEBCONTENTSKEY", webContents);
 * // ... send to other location ...
 * bundle.setClassLoader(WebContents.class.getClassLoader());
 * webContents = bundle.get("WEBCONTENTSKEY");
 */
public interface WebContents extends Parcelable {
    /**
     * Deletes the Web Contents object.
     */
    void destroy();

    /**
     * @return Whether or not the native object associated with this WebContent is destroyed.
     */
    boolean isDestroyed();

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
     * Cut the selected content.
     */
    void cut();

    /**
     * Copy the selected content.
     */
    void copy();

    /**
     * Paste content from the clipboard.
     */
    void paste();

    /**
     * Replace the selected text with the {@code word}.
     */
    void replace(String word);

    /**
     * Select all content.
     */
    void selectAll();

    /**
     * Clear the selection. This includes the cursor which is a zero-sized selection, and keyboard
     * will be hidden as a result.
     */
    void unselect();

    /**
     * Inserts css into main frame's document.
     */
    void insertCSS(String css);

    /**
     * To be called when the ContentView is hidden.
     */
    void onHide();

    /**
     * To be called when the ContentView is shown.
     */
    void onShow();

    /**
     * Stops all media players for this WebContents.
     */
    void releaseMediaPlayers();

    /**
     * Get the Background color from underlying RenderWidgetHost for this WebContent.
     */
    int getBackgroundColor();

    /**
     * Shows an interstitial page driven by the passed in delegate.
     *
     * @param url The URL being blocked by the interstitial.
     * @param interstitialPageDelegateAndroid The delegate handling the interstitial.
     */
    @VisibleForTesting
    void showInterstitialPage(
            String url, long interstitialPageDelegateAndroid);

    /**
     * @return Whether the page is currently showing an interstitial, such as a bad HTTPS page.
     */
    boolean isShowingInterstitialPage();

    /**
     * @return Whether the location bar should be focused by default for this page.
     */
    boolean focusLocationBarByDefault();

    /**
     * If the view is ready to draw contents to the screen. In hardware mode,
     * the initialization of the surface texture may not occur until after the
     * view has been added to the layout. This method will return {@code true}
     * once the texture is actually ready.
     */
    boolean isReady();

     /**
     * Inform WebKit that Fullscreen mode has been exited by the user.
     */
    void exitFullscreen();

    /**
     * Changes whether hiding the top controls is enabled.
     *
     * @param enableHiding Whether hiding the top controls should be enabled or not.
     * @param enableShowing Whether showing the top controls should be enabled or not.
     * @param animate Whether the transition should be animated or not.
     */
    void updateTopControlsState(boolean enableHiding, boolean enableShowing,
            boolean animate);

    /**
     * Shows the IME if the focused widget could accept text input.
     */
    void showImeIfNeeded();

    /**
     * Brings the Editable to the visible area while IME is up to make easier for inputing text.
     */
    void scrollFocusedEditableNodeIntoView();

    /**
     * Selects the word around the caret, if any.
     * The caller can check if selection actually occurred by listening to OnSelectionChanged.
     */
    void selectWordAroundCaret();

    /**
     * Adjusts the selection starting and ending points by the given amount.
     * A negative amount moves the selection towards the beginning of the document, a positive
     * amount moves the selection towards the end of the document.
     * @param startAdjust The amount to adjust the start of the selection.
     * @param endAdjust The amount to adjust the end of the selection.
     */
    public void adjustSelectionByCharacterOffset(int startAdjust, int endAdjust);

    /**
     * Get the URL of the current page.
     *
     * @return The URL of the current page.
     */
    String getUrl();

    /**
     * Gets the last committed URL. It represents the current page that is
     * displayed in this WebContents. It represents the current security context.
     *
     * @return The last committed URL.
     */
    String getLastCommittedUrl();

    /**
     * Get the InCognito state of WebContents.
     *
     * @return whether this WebContents is in InCognito mode or not
     */
    boolean isIncognito();

    /**
     * Resumes the requests for a newly created window.
     */
    void resumeLoadingCreatedWebContents();

    /**
     * Injects the passed Javascript code in the current page and evaluates it.
     * If a result is required, pass in a callback.
     *
     * It is not possible to use this method to evaluate JavaScript on web
     * content, only on WebUI pages.
     *
     * @param script The Javascript to execute.
     * @param callback The callback to be fired off when a result is ready. The script's
     *                 result will be json encoded and passed as the parameter, and the call
     *                 will be made on the main thread.
     *                 If no result is required, pass null.
     */
    void evaluateJavaScript(String script, JavaScriptCallback callback);

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
    @VisibleForTesting
    void evaluateJavaScriptForTests(String script, JavaScriptCallback callback);

    /**
     * Adds a log message to dev tools console. |level| must be a value of
     * org.chromium.content_public.common.ConsoleMessageLevel.
     */
    void addMessageToDevToolsConsole(int level, String message);

    /**
     * Dispatches a Message event to the specified frame.
     */
    void sendMessageToFrame(String frameName, String message, String targetOrigin);

    /**
     * Returns whether the initial empty page has been accessed by a script from another
     * page. Always false after the first commit.
     *
     * @return Whether the initial empty page has been accessed by a script.
     */
    boolean hasAccessedInitialDocument();

    /**
     * This returns the theme color as set by the theme-color meta tag after getting rid of the
     * alpha.
     * @param defaultColor The default color to be returned if the cached color is not valid.
     * @return The theme color for the content as set by the theme-color meta tag.
     */
    int getThemeColor(int defaultColor);

    /**
     * Requests a snapshop of accessibility tree. The result is provided asynchronously
     * using the callback
     * @param callback The callback to be called when the snapshot is ready. The callback
     *                 cannot be null.
     * @param offsetY The Physical on-screen Y offset amount below the top controls.
     * @param scrollX Horizontal scroll offset in physical pixels
     */
    void requestAccessibilitySnapshot(AccessibilitySnapshotCallback callback, float offsetY,
            float scrollX);

    /**
     * Resumes the current media session.
     */
    void resumeMediaSession();

    /**
     * Suspends the current media session.
     */
    void suspendMediaSession();

    /**
     * Stops the current media session.
     */
    void stopMediaSession();

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

    /**
     * @return The list of observers.
     */
    @VisibleForTesting
    ObserverList.RewindableIterator<WebContentsObserver> getObserversForTesting();

    /**
     * Called when context menu gets opened.
     */
    void onContextMenuOpened();

    /**
     * Called when context menu gets closed. Note that closing context menu that is
     * not triggered by WebContents will still call this. However, it will have no effect
     * if onContextMenuOpened() isn't called in advance.
     */
    void onContextMenuClosed();

    /**
     * @return The character encoding for the current visible page.
     */
    @VisibleForTesting
    String getEncoding();

    public void getContentBitmapAsync(Bitmap.Config config, float scale, Rect srcRect,
            ContentBitmapCallback callback);
    /**
     * Reloads all the Lo-Fi images in this WebContents.
     */
    public void reloadLoFiImages();
}

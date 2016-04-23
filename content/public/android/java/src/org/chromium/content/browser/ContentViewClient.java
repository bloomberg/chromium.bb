// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.view.KeyEvent;
import android.view.View.MeasureSpec;

import org.chromium.base.Log;

/**
 *  Main callback class used by ContentView.
 *
 *  This contains the superset of callbacks required to implement the browser UI and the callbacks
 *  required to implement the WebView API.
 *  The memory and reference ownership of this class is unusual - see the .cc file and ContentView
 *  for more details.
 *
 *  TODO(mkosiba): Rid this guy of default implementations. This class is used by both WebView and
 *  the browser and we don't want a the browser-specific default implementation to accidentally leak
 *  over to WebView.
 */
public class ContentViewClient {
    // Tag used for logging.
    private static final String TAG = "cr.ContentViewClient";

    // Default value to signal that the ContentView's size should not be overridden.
    private static final int UNSPECIFIED_MEASURE_SPEC =
            MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

    public void onUpdateTitle(String title) {
    }

    /**
     * Called whenever the background color of the page changes as notified by WebKit.
     * @param color The new ARGB color of the page background.
     */
    public void onBackgroundColorChanged(int color) {
    }

    /**
     * Notifies the client that the position of the top controls has changed.
     * @param topControlsOffsetYPix The Y offset of the top controls in physical pixels.
     * @param contentOffsetYPix The Y offset of the content in physical pixels.
     */
    public void onOffsetsForFullscreenChanged(
            float topControlsOffsetYPix, float contentOffsetYPix) { }

    public boolean shouldOverrideKeyEvent(KeyEvent event) {
        int keyCode = event.getKeyCode();

        if (!shouldPropagateKey(keyCode)) return true;

        return false;
    }

    /**
     * Called when an ImeEvent is sent to the page. Can be used to know when some text is entered
     * in a page.
     */
    public void onImeEvent() {
    }

    /**
     * Notified when the editability of the focused node changes.
     *
     * @param editable Whether the focused node is editable.
     */
    public void onFocusedNodeEditabilityChanged(boolean editable) {
    }

    /**
     * Called when the contextual ActionBar is shown.
     */
    public void onContextualActionBarShown() {
    }

    /**
     * Called when the contextual ActionBar is hidden.
     */
    public void onContextualActionBarHidden() {
    }

    /**
     * Perform a search on {@code searchQuery}.  This method is only called if
     * {@link #doesPerformWebSearch()} returns {@code true}.
     * @param searchQuery The string to search for.
     */
    public void performWebSearch(String searchQuery) {
    }

    /**
     * If this returns {@code true} contextual web search attempts will be forwarded to
     * {@link #performWebSearch(String)}.
     * @return {@code true} iff this {@link ContentViewClient} wants to consume web search queries
     *         and override the default intent behavior.
     */
    public boolean doesPerformWebSearch() {
        return false;
    }

    /**
     * If this returns {@code true} the text processing intents should be forwarded to {@link
     * startProcessTextIntent(Intent)}, otherwise these intents should be sent by WindowAndroid by
     * default.
     * @return {@code true} iff this {@link ContentViewClient} wants to send the processing intents
     * and override the default intent behavior.
     */
    public boolean doesPerformProcessText() {
        return false;
    }

    /**
     * Send the intent to process the current selected text.
     */
    public void startProcessTextIntent(Intent intent) {}

    /**
     * @param actionModeItem the flag for the action mode item in question. See
     *        {@link WebActionModeCallback.ActionHandler} for a list of valid action
     *        mode item flags.
     * @return true if the action is allowed. Otherwise, the menu item
     *         should be removed from the menu.
     */
    public boolean isSelectActionModeAllowed(int actionModeItem) {
        return true;
    }

    /**
     * Called when a new content intent is requested to be started.
     */
    public void onStartContentIntent(Context context, String intentUrl, boolean isMainFrame) {
        Intent intent;
        // Perform generic parsing of the URI to turn it into an Intent.
        try {
            intent = Intent.parseUri(intentUrl, Intent.URI_INTENT_SCHEME);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        } catch (Exception ex) {
            Log.w(TAG, "Bad URI %s", intentUrl, ex);
            return;
        }

        try {
            context.startActivity(intent);
        } catch (ActivityNotFoundException ex) {
            Log.w(TAG, "No application can handle %s", intentUrl);
        }
    }

    public ContentVideoViewEmbedder getContentVideoViewEmbedder() {
        return null;
    }

    /**
     * Called when BrowserMediaPlayerManager wants to load a media resource.
     * @param url the URL of media resource to load.
     * @return true to prevent the resource from being loaded.
     */
    public boolean shouldBlockMediaRequest(String url) {
        return false;
    }

    /**
     * Check whether a key should be propagated to the embedder or not.
     * We need to send almost every key to Blink. However:
     * 1. We don't want to block the device on the renderer for
     * some keys like menu, home, call.
     * 2. There are no WebKit equivalents for some of these keys
     * (see app/keyboard_codes_win.h)
     * Note that these are not the same set as KeyEvent.isSystemKey:
     * for instance, AKEYCODE_MEDIA_* will be dispatched to webkit*.
     */
    public static boolean shouldPropagateKey(int keyCode) {
        if (keyCode == KeyEvent.KEYCODE_MENU
                || keyCode == KeyEvent.KEYCODE_HOME
                || keyCode == KeyEvent.KEYCODE_BACK
                || keyCode == KeyEvent.KEYCODE_CALL
                || keyCode == KeyEvent.KEYCODE_ENDCALL
                || keyCode == KeyEvent.KEYCODE_POWER
                || keyCode == KeyEvent.KEYCODE_HEADSETHOOK
                || keyCode == KeyEvent.KEYCODE_CAMERA
                || keyCode == KeyEvent.KEYCODE_FOCUS
                || keyCode == KeyEvent.KEYCODE_VOLUME_DOWN
                || keyCode == KeyEvent.KEYCODE_VOLUME_MUTE
                || keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
            return false;
        }
        return true;
    }

    /**
     * ContentViewClient users can return a custom value to override the width of
     * the ContentView. By default, this method returns MeasureSpec.UNSPECIFIED, which
     * indicates that the value should not be overridden.
     *
     * @return The desired width of the ContentView.
     */
    public int getDesiredWidthMeasureSpec() {
        return UNSPECIFIED_MEASURE_SPEC;
    }

    /**
     * ContentViewClient users can return a custom value to override the height of
     * the ContentView. By default, this method returns MeasureSpec.UNSPECIFIED, which
     * indicates that the value should not be overridden.
     *
     * @return The desired height of the ContentView.
     */
    public int getDesiredHeightMeasureSpec() {
        return UNSPECIFIED_MEASURE_SPEC;
    }

    /**
     * Returns the left system window inset in pixels. The system window inset represents the area
     * of a full-screen window that is partially or fully obscured by the status bar, navigation
     * bar, IME or other system windows.
     * @return The left system window inset.
     */
    public int getSystemWindowInsetLeft() {
        return 0;
    }

    /**
     * Returns the top system window inset in pixels. The system window inset represents the area of
     * a full-screen window that is partially or fully obscured by the status bar, navigation bar,
     * IME or other system windows.
     * @return The top system window inset.
     */
    public int getSystemWindowInsetTop() {
        return 0;
    }

    /**
     * Returns the right system window inset in pixels. The system window inset represents the area
     * of a full-screen window that is partially or fully obscured by the status bar, navigation
     * bar, IME or other system windows.
     * @return The right system window inset.
     */
    public int getSystemWindowInsetRight() {
        return 0;
    }

    /**
     * Returns the bottom system window inset in pixels. The system window inset represents the area
     * of a full-screen window that is partially or fully obscured by the status bar, navigation
     * bar, IME or other system windows.
     * @return The bottom system window inset.
     */
    public int getSystemWindowInsetBottom() {
        return 0;
    }

    /**
     * Return the product version.
     */
    public String getProductVersion() {
        return "";
    }
}

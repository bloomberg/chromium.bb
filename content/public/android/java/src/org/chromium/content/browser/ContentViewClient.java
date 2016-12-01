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
import org.chromium.base.metrics.RecordUserAction;

/**
 *  Main callback class used by ContentView.
 *
 *  This contains the superset of callbacks required to implement the browser UI and the callbacks
 *  required to implement the WebView API.
 *  The memory and reference ownership of this class is unusual - see the .cc file and ContentView
 *  for more details.
 *
 *  TODO(mkosiba): Rid this class of default implementations. This class is used by both WebView and
 *  the browser and we don't want a the browser-specific default implementation to accidentally leak
 *  over to WebView.
 */
public class ContentViewClient {
    // Tag used for logging.
    private static final String TAG = "cr_ContentViewClient";

    // Default value to signal that the ContentView's size should not be overridden.
    private static final int UNSPECIFIED_MEASURE_SPEC =
            MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

    private static final String GEO_SCHEME = "geo";
    private static final String TEL_SCHEME = "tel";
    private static final String MAILTO_SCHEME = "mailto";

    /**
     * Called whenever the background color of the page changes as notified by WebKit.
     * @param color The new ARGB color of the page background.
     */
    public void onBackgroundColorChanged(int color) {
    }

    /**
     * Notifies the client of the position of the top controls.
     * @param topControlsOffsetY The Y offset of the top controls in physical pixels.
     * @param topContentOffsetY The Y offset of the content in physical pixels.
     */
    public void onTopControlsChanged(float browserControlsOffsetY, float topContentOffsetY) {}

    /**
     * Notifies the client of the position of the bottom controls.
     * @param bottomControlsOffsetY The Y offset of the bottom controls in physical pixels.
     * @param bottomContentOffsetY The Y offset of the content in physical pixels.
     */
    public void onBottomControlsChanged(float bottomControlsOffsetY, float bottomContentOffsetY) { }

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
     * Check whether the given scheme is one of the acceptable schemes for onStartContentIntent.
     *
     * @param scheme The scheme to check.
     * @return true if the scheme is okay, false if it should be blocked.
     */
    protected boolean isAcceptableContentIntentScheme(String scheme) {
        return GEO_SCHEME.equals(scheme) || TEL_SCHEME.equals(scheme)
                || MAILTO_SCHEME.equals(scheme);
    }

    /**
     * Called when a new content intent is requested to be started.
     */
    public void onStartContentIntent(Context context, String intentUrl, boolean isMainFrame) {
        Intent intent;
        // Perform generic parsing of the URI to turn it into an Intent.
        try {
            intent = Intent.parseUri(intentUrl, Intent.URI_INTENT_SCHEME);

            String scheme = intent.getScheme();
            if (!isAcceptableContentIntentScheme(scheme)) {
                Log.w(TAG, "Invalid scheme for URI %s", intentUrl);
                return;
            }

            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        } catch (Exception ex) {
            Log.w(TAG, "Bad URI %s", intentUrl, ex);
            return;
        }

        try {
            RecordUserAction.record("Android.ContentDetectorActivated");
            context.startActivity(intent);
        } catch (ActivityNotFoundException ex) {
            Log.w(TAG, "No application can handle %s", intentUrl);
        }
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
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.graphics.Rect;
import android.graphics.RectF;
import android.util.Log;
import android.view.KeyEvent;

import java.net.URISyntaxException;

import org.chromium.base.AccessedByNative;
import org.chromium.base.CalledByNative;

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
    private static final String TAG = "ContentViewClient";

    // Native class pointer which will be set by nativeInit()
    @AccessedByNative
    private int mNativeClazz = 0;

    // These ints must match up to the native values in content_view_client.h.
    // Generic error
    public static final int ERROR_UNKNOWN = -1;
    // Server or proxy hostname lookup failed
    public static final int ERROR_HOST_LOOKUP = -2;
    // Unsupported authentication scheme (not basic or digest)
    public static final int ERROR_UNSUPPORTED_AUTH_SCHEME = -3;
    // User authentication failed on server
    public static final int ERROR_AUTHENTICATION = -4;
    // User authentication failed on proxy
    public static final int ERROR_PROXY_AUTHENTICATION = -5;
    // Failed to connect to the server
    public static final int ERROR_CONNECT = -6;
    // Failed to read or write to the server
    public static final int ERROR_IO = -7;
    // Connection timed out
    public static final int ERROR_TIMEOUT = -8;
    // Too many redirects
    public static final int ERROR_REDIRECT_LOOP = -9;
    // Unsupported URI scheme
    public static final int ERROR_UNSUPPORTED_SCHEME = -10;
    // Failed to perform SSL handshake
    public static final int ERROR_FAILED_SSL_HANDSHAKE = -11;
    // Malformed URL
    public static final int ERROR_BAD_URL = -12;
    // Generic file error
    public static final int ERROR_FILE = -13;
    // File not found
    public static final int ERROR_FILE_NOT_FOUND = -14;
    // Too many requests during this load
    public static final int ERROR_TOO_MANY_REQUESTS = -15;

    @CalledByNative
    public void openNewTab(String url, boolean incognito) {
    }

    @CalledByNative
    public boolean addNewContents(int nativeSourceWebContents, int nativeWebContents,
                                  int disposition, Rect initialPosition, boolean userGesture) {
        return false;
    }

    @CalledByNative
    public void closeContents() {
    }

    @CalledByNative
    public void onUrlStarredChanged(boolean starred) {
    }

    @CalledByNative
    public void onPageStarted(String url) {
    }

    @CalledByNative
    public void onPageFinished(String url) {
    }

    @CalledByNative
    public void onLoadStarted() {
    }

    @CalledByNative
    public void onLoadStopped() {
    }

    @CalledByNative
    public void onReceivedError(int errorCode, String description, String failingUrl) {
    }

    // TODO(jrg): add onReceivedHttpAuthRequest() once ContentHttpAuthHandler is upstreamed

    @CalledByNative
    public void onMainFrameCommitted(String url, String baseUrl) {
    }

    @CalledByNative
    public void onTabHeaderStateChanged() {
    }

    @CalledByNative
    public void onLoadProgressChanged(double progress) {
    }

    public void onUpdateTitle(String title) {
    }

    @CalledByNative
    public void onUpdateUrl(String url) {
    }

    @CalledByNative
    public void onReceiveFindMatchRects(int version, float[] rect_data,
                                        RectF activeRect) {
    }

    @CalledByNative
    public void onInterstitialShown() {
    }

    @CalledByNative
    public void onInterstitialHidden() {
    }

    @CalledByNative
    public boolean takeFocus(boolean reverse) {
        return false;
    }

    public void onTabCrash(int pid) {
    }

    @CalledByNative
    public boolean shouldOverrideUrlLoading(String url) {
        return false;
    }

    public boolean shouldOverrideKeyEvent(KeyEvent event) {
        int keyCode = event.getKeyCode();
        // We need to send almost every key to WebKit. However:
        // 1. We don't want to block the device on the renderer for
        // some keys like menu, home, call.
        // 2. There are no WebKit equivalents for some of these keys
        // (see app/keyboard_codes_win.h)
        // Note that these are not the same set as KeyEvent.isSystemKey:
        // for instance, AKEYCODE_MEDIA_* will be dispatched to webkit.
        if (keyCode == KeyEvent.KEYCODE_MENU ||
            keyCode == KeyEvent.KEYCODE_HOME ||
            keyCode == KeyEvent.KEYCODE_BACK ||
            keyCode == KeyEvent.KEYCODE_CALL ||
            keyCode == KeyEvent.KEYCODE_ENDCALL ||
            keyCode == KeyEvent.KEYCODE_POWER ||
            keyCode == KeyEvent.KEYCODE_HEADSETHOOK ||
            keyCode == KeyEvent.KEYCODE_CAMERA ||
            keyCode == KeyEvent.KEYCODE_FOCUS ||
            keyCode == KeyEvent.KEYCODE_VOLUME_DOWN ||
            keyCode == KeyEvent.KEYCODE_VOLUME_MUTE ||
            keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
            return true;
        }

        // We also have to intercept some shortcuts before we send them to the ContentView.
        if (event.isCtrlPressed() && (
                keyCode == KeyEvent.KEYCODE_TAB ||
                keyCode == KeyEvent.KEYCODE_W ||
                keyCode == KeyEvent.KEYCODE_F4)) {
            return true;
        }

        return false;
    }

    // Called when an ImeEvent is sent to the page. Can be used to know when some text is entered
    // in a page.
    public void onImeEvent() {
    }

    public void onUnhandledKeyEvent(KeyEvent event) {
        // TODO(bulach): we probably want to re-inject the KeyEvent back into
        // the system. Investigate if this is at all possible.
    }


    @CalledByNative
    public void runFileChooser(FileChooserParams params) {
    }

    // Return true if the client will handle the JS alert.
    @CalledByNative
    public boolean  onJsAlert(String url, String Message) {
        return false;
    }

    // Return true if the client will handle the JS before unload dialog.
    @CalledByNative
    public boolean onJsBeforeUnload(String url, String message) {
        return false;
    }

    // Return true if the client will handle the JS confirmation prompt.
    @CalledByNative
    public boolean onJsConfirm(String url, String message) {
        return false;
    }

    // Return true if the client will handle the JS prompt dialog.
    @CalledByNative
    public boolean onJsPrompt(String url, String message, String defaultValue) {
        return false;
    }

    /**
     * A callback invoked after the JavaScript code passed to evaluateJavaScript
     * has finished execution.
     * Used in automation tests.
     * @hide
     */
    public void onEvaluateJavaScriptResult(int id, String jsonResult) {
    }

    // TODO (dtrainor): Should expose getScrollX/Y from ContentView or make
    // computeHorizontalScrollOffset()/computeVerticalScrollOffset() public.
    /**
     * Gives the UI the chance to override each scroll event.
     * @param dx The amount scrolled in the X direction.
     * @param dy The amount scrolled in the Y direction.
     * @param scrollX The current X scroll offset.
     * @param scrollY The current Y scroll offset.
     * @return Whether or not the UI consumed and handled this event.
     */
    public boolean shouldOverrideScroll(float dx, float dy, float scrollX, float scrollY) {
        return false;
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
     * Called when a new content intent is requested to be started.
     */
    public void onStartContentIntent(ContentView chromeView, String contentUrl) {
        Intent intent;
        // Perform generic parsing of the URI to turn it into an Intent.
        try {
            intent = Intent.parseUri(contentUrl, Intent.URI_INTENT_SCHEME);
        } catch (URISyntaxException ex) {
            Log.w(TAG, "Bad URI " + contentUrl + ": " + ex.getMessage());
            return;
        }

        try {
            chromeView.getContext().startActivity(intent);
        } catch (ActivityNotFoundException ex) {
            Log.w(TAG, "No application can handle " + contentUrl);
        }
    }
}

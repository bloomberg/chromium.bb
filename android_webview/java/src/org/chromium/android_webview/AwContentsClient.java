// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.graphics.Rect;
import android.graphics.RectF;
import android.util.Log;
import android.view.KeyEvent;
import android.webkit.ConsoleMessage;

import org.chromium.content.browser.ContentViewClient;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.WebContentsObserverAndroid;

/**
 * Base-class that an AwContents embedder derives from to receive callbacks.
 * This extends ContentViewClient, as in many cases we want to pass-thru ContentViewCore
 * callbacks right to our embedder, and this setup facilities that.
 * For any other callbacks we need to make transformations of (e.g. adapt parameters
 * or perform filtering) we can provide final overrides for methods here, and then introduce
 * new abstract methods that the our own client must implement.
 * i.e.: all methods in this class should either be final, or abstract.
 */
public abstract class AwContentsClient extends ContentViewClient {

    private static final String TAG = "AwContentsClient";
    // Handler for WebContentsDelegate callbacks
    private final WebContentsDelegateAdapter mWebContentsDelegateAdapter =
            new WebContentsDelegateAdapter();

    private AwWebContentsObserver mWebContentsObserver;

    //--------------------------------------------------------------------------------------------
    //                        Adapter for WebContentsDelegate methods.
    //--------------------------------------------------------------------------------------------

    class WebContentsDelegateAdapter extends AwWebContentsDelegate {
        @Override
        public void onLoadProgressChanged(int progress) {
            AwContentsClient.this.onProgressChanged(progress);
        }

        @Override
        public boolean shouldOverrideUrlLoading(String url) {
            return AwContentsClient.this.shouldOverrideUrlLoading(url);
        }

        @Override
        public void handleKeyboardEvent(KeyEvent event) {
            AwContentsClient.this.onUnhandledKeyEvent(event);
        }

        @Override
        public boolean addMessageToConsole(int level, String message, int lineNumber,
                String sourceId) {
            ConsoleMessage.MessageLevel messageLevel = ConsoleMessage.MessageLevel.DEBUG;
            switch(level) {
                case LOG_LEVEL_TIP:
                    messageLevel = ConsoleMessage.MessageLevel.TIP;
                    break;
                case LOG_LEVEL_LOG:
                    messageLevel = ConsoleMessage.MessageLevel.LOG;
                    break;
                case LOG_LEVEL_WARNING:
                    messageLevel = ConsoleMessage.MessageLevel.WARNING;
                    break;
                case LOG_LEVEL_ERROR:
                    messageLevel = ConsoleMessage.MessageLevel.ERROR;
                    break;
                default:
                    Log.w(TAG, "Unknown message level, defaulting to DEBUG");
                    break;
            }

            return AwContentsClient.this.onConsoleMessage(
                    new ConsoleMessage(message, sourceId, lineNumber, messageLevel));
        }

        @Override
        public void onUpdateUrl(String url) {
            // TODO: implement
        }

        @Override
        public void openNewTab(String url, boolean incognito) {
            // TODO: implement
        }

        @Override
        public boolean addNewContents(int nativeSourceWebContents, int nativeWebContents,
                int disposition, Rect initialPosition, boolean userGesture) {
            // TODO: implement
            return false;
        }

        @Override
        public void closeContents() {
            // TODO: implement
        }

        @Override
        public void onUrlStarredChanged(boolean starred) {
            // TODO: implement
        }
    }

    class AwWebContentsObserver extends WebContentsObserverAndroid {
        public AwWebContentsObserver(ContentViewCore contentViewCore) {
            super(contentViewCore);
        }

        @Override
        public void didStartLoading(String url) {
            AwContentsClient.this.onPageStarted(url);
        }

        @Override
        public void didStopLoading(String url) {
            AwContentsClient.this.onPageFinished(url);
        }

        @Override
        public void didFailLoad(boolean isProvisionalLoad,
                boolean isMainFrame, int errorCode, String description, String failingUrl) {
            AwContentsClient.this.onReceivedError(
                    ErrorCodeConversionHelper.convertErrorCode(errorCode), description, failingUrl);
        }
    }

    void installWebContentsObserver(ContentViewCore contentViewCore) {
        assert mWebContentsObserver == null;
        mWebContentsObserver = new AwWebContentsObserver(contentViewCore);
    }

    final AwWebContentsDelegate getWebContentsDelegate()  {
        return mWebContentsDelegateAdapter;
    }

    //--------------------------------------------------------------------------------------------
    //             WebView specific methods that map directly to WebViewClient / WebChromeClient
    //--------------------------------------------------------------------------------------------
    //

    public abstract void onProgressChanged(int progress);

    public abstract boolean shouldOverrideUrlLoading(String url);

    public abstract void onUnhandledKeyEvent(KeyEvent event);

    public abstract boolean onConsoleMessage(ConsoleMessage consoleMessage);

    public abstract void onReceivedHttpAuthRequest(AwHttpAuthHandler handler,
            String host, String realm);

    protected abstract void handleJsAlert(String url, String message, JsResultReceiver receiver);

    protected abstract void handleJsBeforeUnload(String url, String message,
                                                 JsResultReceiver receiver);

    protected abstract void handleJsConfirm(String url, String message, JsResultReceiver receiver);

    protected abstract void handleJsPrompt(String url, String message, String defaultValue,
            JsPromptResultReceiver receiver);

    //--------------------------------------------------------------------------------------------
    //                              Other WebView-specific methods
    //--------------------------------------------------------------------------------------------
    //

    public abstract void onFindResultReceived(int activeMatchOrdinal, int numberOfMatches,
            boolean isDoneCounting);

    public abstract void onPageStarted(String url);

    public abstract void onPageFinished(String url);

    public abstract void onReceivedError(int errorCode, String description, String failingUrl);

    //--------------------------------------------------------------------------------------------
    //             Stuff that we ignore since it only makes sense for Chrome browser
    //--------------------------------------------------------------------------------------------
    //

    @Override
    final public boolean shouldOverrideScroll(float dx, float dy, float scrollX, float scrollY) {
        return false;
    }

    @Override
    final public void onContextualActionBarShown() {
    }

    @Override
    final public void onContextualActionBarHidden() {
    }
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.Log;
import android.view.KeyEvent;
import android.webkit.ConsoleMessage;
import android.webkit.GeolocationPermissions;

import org.chromium.content.browser.ContentViewClient;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.WebContentsObserverAndroid;
import org.chromium.net.NetError;

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

    private final AwContentsClientCallbackHelper mCallbackHelper =
        new AwContentsClientCallbackHelper(this);

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
            AwContentsClient.this.onCloseWindow();
        }

        @Override
        public void showRepostFormWarningDialog(final ContentViewCore contentViewCore) {
            // This is intentionally not part of mCallbackHelper as that class is intended for
            // callbacks going the other way (to the embedder, not from the embedder).
            // TODO(mkosiba) We should be using something akin to the JsResultReceiver as the
            // callback parameter (instead of ContentViewCore) and implement a way of converting
            // that to a pair of messages.
            final int MSG_CONTINUE_PENDING_RELOAD = 1;
            final int MSG_CANCEL_PENDING_RELOAD = 2;

            // TODO(sgurun) Remember the URL to cancel the reload behavior
            // if it is different than the most recent NavigationController entry.
            final Handler handler = new Handler(Looper.getMainLooper()) {
                @Override
                public void handleMessage(Message msg) {
                    switch(msg.what) {
                        case MSG_CONTINUE_PENDING_RELOAD: {
                            contentViewCore.continuePendingReload();
                            break;
                        }
                        case MSG_CANCEL_PENDING_RELOAD: {
                            contentViewCore.cancelPendingReload();
                            break;
                        }
                        default:
                            throw new IllegalStateException(
                                    "WebContentsDelegateAdapter: unhandled message " + msg.what);
                    }
                }
            };

            Message resend = handler.obtainMessage(MSG_CONTINUE_PENDING_RELOAD);
            Message dontResend = handler.obtainMessage(MSG_CANCEL_PENDING_RELOAD);
            AwContentsClient.this.onFormResubmission(dontResend, resend);
        }

        @Override
        public boolean addNewContents(boolean isDialog, boolean isUserGesture) {
            return AwContentsClient.this.onCreateWindow(isDialog, isUserGesture);
        }

        @Override
        public void activateContents() {
            AwContentsClient.this.onRequestFocus();
        }
    }

    class AwWebContentsObserver extends WebContentsObserverAndroid {
        public AwWebContentsObserver(ContentViewCore contentViewCore) {
            super(contentViewCore);
        }

        @Override
        public void didStopLoading(String url) {
            AwContentsClient.this.onPageFinished(url);
        }

        @Override
        public void didFailLoad(boolean isProvisionalLoad,
                boolean isMainFrame, int errorCode, String description, String failingUrl) {
            if (errorCode == NetError.ERR_ABORTED) {
                // This error code is generated for the following reasons:
                // - WebView.stopLoading is called,
                // - the navigation is intercepted by the embedder via shouldIgnoreNavigation.
                //
                // The Android WebView does not notify the embedder of these situations using this
                // error code with the WebViewClient.onReceivedError callback.
                return;
            }
            if (!isMainFrame) {
                // The Android WebView does not notify the embedder of sub-frame failures.
                return;
            }
            AwContentsClient.this.onReceivedError(
                    ErrorCodeConversionHelper.convertErrorCode(errorCode), description, failingUrl);
        }

        @Override
        public void didNavigateAnyFrame(String url, String baseUrl, boolean isReload) {
            AwContentsClient.this.doUpdateVisitedHistory(url, isReload);
        }

    }

    void installWebContentsObserver(ContentViewCore contentViewCore) {
        if (mWebContentsObserver != null) {
            mWebContentsObserver.detachFromWebContents();
        }
        mWebContentsObserver = new AwWebContentsObserver(contentViewCore);
    }

    final AwWebContentsDelegate getWebContentsDelegate() {
        return mWebContentsDelegateAdapter;
    }

    final AwContentsClientCallbackHelper getCallbackHelper() {
        return mCallbackHelper;
    }

    //--------------------------------------------------------------------------------------------
    //             WebView specific methods that map directly to WebViewClient / WebChromeClient
    //--------------------------------------------------------------------------------------------

    // TODO(boliu): Make this abstract.
    public void doUpdateVisitedHistory(String url, boolean isReload) {}

    public abstract void onProgressChanged(int progress);

    public abstract InterceptedRequestData shouldInterceptRequest(String url);

    public abstract void onLoadResource(String url);

    public abstract boolean shouldIgnoreNavigation(String url);

    public abstract void onUnhandledKeyEvent(KeyEvent event);

    public abstract boolean onConsoleMessage(ConsoleMessage consoleMessage);

    public abstract void onReceivedHttpAuthRequest(AwHttpAuthHandler handler,
            String host, String realm);

    public abstract void onFormResubmission(Message dontResend, Message resend);

    public abstract void onDownloadStart(String url, String userAgent, String contentDisposition,
            String mimeType, long contentLength);

    public abstract void onGeolocationPermissionsShowPrompt(String origin,
            GeolocationPermissions.Callback callback);

    public abstract void onGeolocationPermissionsHidePrompt();

    protected abstract void handleJsAlert(String url, String message, JsResultReceiver receiver);

    protected abstract void handleJsBeforeUnload(String url, String message,
            JsResultReceiver receiver);

    protected abstract void handleJsConfirm(String url, String message, JsResultReceiver receiver);

    protected abstract void handleJsPrompt(String url, String message, String defaultValue,
            JsPromptResultReceiver receiver);

    protected abstract boolean onCreateWindow(boolean isDialog, boolean isUserGesture);

    protected abstract void onCloseWindow();

    // TODO(acleung): Make abstract when landed in Android
    public void onReceivedTouchIconUrl(String url, boolean precomposed) { }

    // TODO(acleung): Make abstract when landed in Android
    public void onReceivedIcon(Bitmap bitmap) { }

    protected abstract void onRequestFocus();

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

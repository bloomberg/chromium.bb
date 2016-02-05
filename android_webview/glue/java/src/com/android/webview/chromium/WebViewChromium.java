// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.assist.AssistStructure.ViewNode;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Picture;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.net.http.SslCertificate;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.print.PrintDocumentAdapter;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStructure;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeProvider;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.webkit.DownloadListener;
import android.webkit.FindActionModeCallback;
import android.webkit.ValueCallback;
import android.webkit.WebBackForwardList;
import android.webkit.WebChromeClient;
import android.webkit.WebChromeClient.CustomViewCallback;
import android.webkit.WebMessage;
import android.webkit.WebMessagePort;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebView.VisualStateCallback;
import android.webkit.WebViewClient;
import android.webkit.WebViewProvider;
import android.widget.TextView;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.AwPrintDocumentAdapter;
import org.chromium.android_webview.AwSettings;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.content.browser.SmartClipProvider;
import org.chromium.content_public.browser.AccessibilitySnapshotCallback;
import org.chromium.content_public.browser.AccessibilitySnapshotNode;
import org.chromium.content_public.browser.NavigationHistory;

import java.io.BufferedWriter;
import java.io.File;
import java.lang.reflect.Method;
import java.util.Iterator;
import java.util.Map;
import java.util.Queue;
import java.util.concurrent.Callable;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;

/**
 * This class is the delegate to which WebViewProxy forwards all API calls.
 *
 * Most of the actual functionality is implemented by AwContents (or ContentViewCore within
 * it). This class also contains WebView-specific APIs that require the creation of other
 * adapters (otherwise org.chromium.content would depend on the webview.chromium package)
 * and a small set of no-op deprecated APIs.
 */
@SuppressWarnings("deprecation")
class WebViewChromium implements WebViewProvider, WebViewProvider.ScrollDelegate,
                                 WebViewProvider.ViewDelegate, SmartClipProvider {
    private class WebViewChromiumRunQueue {
        public WebViewChromiumRunQueue() {
            mQueue = new ConcurrentLinkedQueue<Runnable>();
        }

        public void addTask(Runnable task) {
            mQueue.add(task);
            if (mFactory.hasStarted()) {
                ThreadUtils.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        drainQueue();
                    }
                });
            }
        }

        public void drainQueue() {
            if (mQueue == null || mQueue.isEmpty()) {
                return;
            }

            Runnable task = mQueue.poll();
            while (task != null) {
                task.run();
                task = mQueue.poll();
            }
        }

        private Queue<Runnable> mQueue;
    }

    private WebViewChromiumRunQueue mRunQueue;

    private static final String TAG = WebViewChromium.class.getSimpleName();

    // The WebView that this WebViewChromium is the provider for.
    WebView mWebView;
    // Lets us access protected View-derived methods on the WebView instance we're backing.
    WebView.PrivateAccess mWebViewPrivate;
    // The client adapter class.
    private WebViewContentsClientAdapter mContentsClientAdapter;
    // The wrapped Context.
    private Context mContext;

    // Variables for functionality provided by this adapter ---------------------------------------
    private ContentSettingsAdapter mWebSettings;
    // The WebView wrapper for ContentViewCore and required browser compontents.
    private AwContents mAwContents;
    // Non-null if this webview is using the GL accelerated draw path.
    private DrawGLFunctor mGLfunctor;

    private final WebView.HitTestResult mHitTestResult;

    private final int mAppTargetSdkVersion;

    private WebViewChromiumFactoryProvider mFactory;

    private static boolean sRecordWholeDocumentEnabledByApi = false;
    static void enableSlowWholeDocumentDraw() {
        sRecordWholeDocumentEnabledByApi = true;
    }

    // This does not touch any global / non-threadsafe state, but note that
    // init is ofter called right after and is NOT threadsafe.
    public WebViewChromium(WebViewChromiumFactoryProvider factory, WebView webView,
            WebView.PrivateAccess webViewPrivate) {
        mWebView = webView;
        mWebViewPrivate = webViewPrivate;
        mHitTestResult = new WebView.HitTestResult();
        mContext = ResourcesContextWrapperFactory.get(mWebView.getContext());
        mAppTargetSdkVersion = mContext.getApplicationInfo().targetSdkVersion;
        mFactory = factory;
        mRunQueue = new WebViewChromiumRunQueue();
        factory.getWebViewDelegate().addWebViewAssetPath(mWebView.getContext());
    }

    static void completeWindowCreation(WebView parent, WebView child) {
        AwContents parentContents = ((WebViewChromium) parent.getWebViewProvider()).mAwContents;
        AwContents childContents =
                child == null ? null : ((WebViewChromium) child.getWebViewProvider()).mAwContents;
        parentContents.supplyContentsForPopup(childContents);
    }

    private <T> T runBlockingFuture(FutureTask<T> task) {
        if (!mFactory.hasStarted()) throw new RuntimeException("Must be started before we block!");
        if (ThreadUtils.runningOnUiThread()) {
            throw new IllegalStateException("This method should only be called off the UI thread");
        }
        mRunQueue.addTask(task);
        try {
            return task.get(4, TimeUnit.SECONDS);
        } catch (java.util.concurrent.TimeoutException e) {
            throw new RuntimeException("Probable deadlock detected due to WebView API being called "
                            + "on incorrect thread while the UI thread is blocked.", e);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    // We have a 4 second timeout to try to detect deadlocks to detect and aid in debuggin
    // deadlocks.
    // Do not call this method while on the UI thread!
    private void runVoidTaskOnUiThreadBlocking(Runnable r) {
        FutureTask<Void> task = new FutureTask<Void>(r, null);
        runBlockingFuture(task);
    }

    private <T> T runOnUiThreadBlocking(Callable<T> c) {
        return runBlockingFuture(new FutureTask<T>(c));
    }

    // WebViewProvider methods --------------------------------------------------------------------

    @Override
    // BUG=6790250 |javaScriptInterfaces| was only ever used by the obsolete DumpRenderTree
    // so is ignored. TODO: remove it from WebViewProvider.
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public void init(final Map<String, Object> javaScriptInterfaces,
            final boolean privateBrowsing) {
        if (privateBrowsing) {
            mFactory.startYourEngines(true);
            final String msg = "Private browsing is not supported in WebView.";
            if (mAppTargetSdkVersion >= Build.VERSION_CODES.KITKAT) {
                throw new IllegalArgumentException(msg);
            } else {
                Log.w(TAG, msg);
                TextView warningLabel = new TextView(mContext);
                warningLabel.setText(mContext.getString(
                        org.chromium.android_webview.R.string.private_browsing_warning));
                mWebView.addView(warningLabel);
            }
        }

        // We will defer real initialization until we know which thread to do it on, unless:
        // - we are on the main thread already (common case),
        // - the app is targeting >= JB MR2, in which case checkThread enforces that all usage
        //   comes from a single thread. (Note in JB MR2 this exception was in WebView.java).
        if (mAppTargetSdkVersion >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
            mFactory.startYourEngines(false);
            checkThread();
        } else if (!mFactory.hasStarted()) {
            if (Looper.myLooper() == Looper.getMainLooper()) {
                mFactory.startYourEngines(true);
            }
        }

        final boolean isAccessFromFileURLsGrantedByDefault =
                mAppTargetSdkVersion < Build.VERSION_CODES.JELLY_BEAN;
        final boolean areLegacyQuirksEnabled = mAppTargetSdkVersion < Build.VERSION_CODES.KITKAT;

        mContentsClientAdapter =
                new WebViewContentsClientAdapter(mWebView, mContext, mFactory.getWebViewDelegate());
        mWebSettings = new ContentSettingsAdapter(new AwSettings(
                mContext, isAccessFromFileURLsGrantedByDefault, areLegacyQuirksEnabled));

        if (mAppTargetSdkVersion < Build.VERSION_CODES.LOLLIPOP) {
            // Prior to Lollipop we always allowed third party cookies and mixed content.
            mWebSettings.setMixedContentMode(WebSettings.MIXED_CONTENT_ALWAYS_ALLOW);
            mWebSettings.setAcceptThirdPartyCookies(true);
            mWebSettings.getAwSettings().setZeroLayoutHeightDisablesViewportQuirk(true);
        }

        mRunQueue.addTask(new Runnable() {
            @Override
            public void run() {
                initForReal();
                if (privateBrowsing) {
                    // Intentionally irreversibly disable the webview instance, so that private
                    // user data cannot leak through misuse of a non-privateBrowing WebView
                    // instance. Can't just null out mAwContents as we never null-check it
                    // before use.
                    destroy();
                }
            }
        });
    }

    private void initForReal() {
        AwContentsStatics.setRecordFullDocument(sRecordWholeDocumentEnabledByApi
                || mAppTargetSdkVersion < Build.VERSION_CODES.LOLLIPOP);

        mAwContents = new AwContents(mFactory.getBrowserContext(), mWebView, mContext,
                new InternalAccessAdapter(), new WebViewNativeGLDelegate(), mContentsClientAdapter,
                mWebSettings.getAwSettings());

        if (mAppTargetSdkVersion >= Build.VERSION_CODES.KITKAT) {
            // On KK and above, favicons are automatically downloaded as the method
            // old apps use to enable that behavior is deprecated.
            AwContents.setShouldDownloadFavicons();
        }

        if (mAppTargetSdkVersion < Build.VERSION_CODES.LOLLIPOP) {
            // Prior to Lollipop, JavaScript objects injected via addJavascriptInterface
            // were not inspectable.
            mAwContents.disableJavascriptInterfacesInspection();
        }

        // TODO: This assumes AwContents ignores second Paint param.
        mAwContents.setLayerType(mWebView.getLayerType(), null);
    }

    void startYourEngine() {
        mRunQueue.drainQueue();
    }

    private RuntimeException createThreadException() {
        return new IllegalStateException(
                "Calling View methods on another thread than the UI thread.");
    }

    private boolean checkNeedsPost() {
        boolean needsPost = !mFactory.hasStarted() || !ThreadUtils.runningOnUiThread();
        if (!needsPost && mAwContents == null) {
            throw new IllegalStateException("AwContents must be created if we are not posting!");
        }
        return needsPost;
    }

    //  Intentionally not static, as no need to check thread on static methods
    private void checkThread() {
        if (!ThreadUtils.runningOnUiThread()) {
            final RuntimeException threadViolation = createThreadException();
            ThreadUtils.postOnUiThread(new Runnable() {
                @Override
                public void run() {
                    throw threadViolation;
                }
            });
            throw createThreadException();
        }
    }

    @Override
    public void setHorizontalScrollbarOverlay(final boolean overlay) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    setHorizontalScrollbarOverlay(overlay);
                }
            });
            return;
        }
        mAwContents.setHorizontalScrollbarOverlay(overlay);
    }

    @Override
    public void setVerticalScrollbarOverlay(final boolean overlay) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    setVerticalScrollbarOverlay(overlay);
                }
            });
            return;
        }
        mAwContents.setVerticalScrollbarOverlay(overlay);
    }

    @Override
    public boolean overlayHorizontalScrollbar() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret = runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return overlayHorizontalScrollbar();
                }
            });
            return ret;
        }
        return mAwContents.overlayHorizontalScrollbar();
    }

    @Override
    public boolean overlayVerticalScrollbar() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret = runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return overlayVerticalScrollbar();
                }
            });
            return ret;
        }
        return mAwContents.overlayVerticalScrollbar();
    }

    @Override
    public int getVisibleTitleHeight() {
        // This is deprecated in WebView and should always return 0.
        return 0;
    }

    @Override
    public SslCertificate getCertificate() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            SslCertificate ret = runOnUiThreadBlocking(new Callable<SslCertificate>() {
                @Override
                public SslCertificate call() {
                    return getCertificate();
                }
            });
            return ret;
        }
        return mAwContents.getCertificate();
    }

    @Override
    public void setCertificate(SslCertificate certificate) {
        // intentional no-op
    }

    @Override
    public void savePassword(String host, String username, String password) {
        // This is a deprecated API: intentional no-op.
    }

    @Override
    public void setHttpAuthUsernamePassword(
            final String host, final String realm, final String username, final String password) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    setHttpAuthUsernamePassword(host, realm, username, password);
                }
            });
            return;
        }
        mAwContents.setHttpAuthUsernamePassword(host, realm, username, password);
    }

    @Override
    public String[] getHttpAuthUsernamePassword(final String host, final String realm) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            String[] ret = runOnUiThreadBlocking(new Callable<String[]>() {
                @Override
                public String[] call() {
                    return getHttpAuthUsernamePassword(host, realm);
                }
            });
            return ret;
        }
        return mAwContents.getHttpAuthUsernamePassword(host, realm);
    }

    @Override
    public void destroy() {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    destroy();
                }
            });
            return;
        }

        // Make sure that we do not trigger any callbacks after destruction
        mContentsClientAdapter.setWebChromeClient(null);
        mContentsClientAdapter.setWebViewClient(null);
        mContentsClientAdapter.setPictureListener(null);
        mContentsClientAdapter.setFindListener(null);
        mContentsClientAdapter.setDownloadListener(null);

        mAwContents.destroy();
        if (mGLfunctor != null) {
            mGLfunctor.destroy();
            mGLfunctor = null;
        }
    }

    @Override
    public void setNetworkAvailable(final boolean networkUp) {
        // Note that this purely toggles the JS navigator.online property.
        // It does not in affect chromium or network stack state in any way.
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    setNetworkAvailable(networkUp);
                }
            });
            return;
        }
        mAwContents.setNetworkAvailable(networkUp);
    }

    @Override
    public WebBackForwardList saveState(final Bundle outState) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            WebBackForwardList ret = runOnUiThreadBlocking(new Callable<WebBackForwardList>() {
                @Override
                public WebBackForwardList call() {
                    return saveState(outState);
                }
            });
            return ret;
        }
        if (outState == null) return null;
        if (!mAwContents.saveState(outState)) return null;
        return copyBackForwardList();
    }

    @Override
    public boolean savePicture(Bundle b, File dest) {
        // Intentional no-op: hidden method on WebView.
        return false;
    }

    @Override
    public boolean restorePicture(Bundle b, File src) {
        // Intentional no-op: hidden method on WebView.
        return false;
    }

    @Override
    public WebBackForwardList restoreState(final Bundle inState) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            WebBackForwardList ret = runOnUiThreadBlocking(new Callable<WebBackForwardList>() {
                @Override
                public WebBackForwardList call() {
                    return restoreState(inState);
                }
            });
            return ret;
        }
        if (inState == null) return null;
        if (!mAwContents.restoreState(inState)) return null;
        return copyBackForwardList();
    }

    @Override
    public void loadUrl(final String url, final Map<String, String> additionalHttpHeaders) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            // Disallowed in WebView API for apps targetting a new SDK
            assert mAppTargetSdkVersion < Build.VERSION_CODES.JELLY_BEAN_MR2;
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    mAwContents.loadUrl(url, additionalHttpHeaders);
                }
            });
            return;
        }
        mAwContents.loadUrl(url, additionalHttpHeaders);
    }

    @Override
    public void loadUrl(final String url) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            // Disallowed in WebView API for apps targetting a new SDK
            assert mAppTargetSdkVersion < Build.VERSION_CODES.JELLY_BEAN_MR2;
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    mAwContents.loadUrl(url);
                }
            });
            return;
        }
        mAwContents.loadUrl(url);
    }

    @Override
    public void postUrl(final String url, final byte[] postData) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            // Disallowed in WebView API for apps targetting a new SDK
            assert mAppTargetSdkVersion < Build.VERSION_CODES.JELLY_BEAN_MR2;
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    mAwContents.postUrl(url, postData);
                }
            });
            return;
        }
        mAwContents.postUrl(url, postData);
    }

    @Override
    public void loadData(final String data, final String mimeType, final String encoding) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            // Disallowed in WebView API for apps targetting a new SDK
            assert mAppTargetSdkVersion < Build.VERSION_CODES.JELLY_BEAN_MR2;
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    mAwContents.loadData(data, mimeType, encoding);
                }
            });
            return;
        }
        mAwContents.loadData(data, mimeType, encoding);
    }

    @Override
    public void loadDataWithBaseURL(final String baseUrl, final String data, final String mimeType,
            final String encoding, final String historyUrl) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            // Disallowed in WebView API for apps targetting a new SDK
            assert mAppTargetSdkVersion < Build.VERSION_CODES.JELLY_BEAN_MR2;
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    mAwContents.loadDataWithBaseURL(baseUrl, data, mimeType, encoding, historyUrl);
                }
            });
            return;
        }
        mAwContents.loadDataWithBaseURL(baseUrl, data, mimeType, encoding, historyUrl);
    }

    public void evaluateJavaScript(String script, ValueCallback<String> resultCallback) {
        checkThread();
        mAwContents.evaluateJavaScript(script, resultCallback);
    }

    @Override
    public void saveWebArchive(String filename) {
        saveWebArchive(filename, false, null);
    }

    @Override
    public void saveWebArchive(final String basename, final boolean autoname,
            final ValueCallback<String> callback) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    saveWebArchive(basename, autoname, callback);
                }
            });
            return;
        }
        mAwContents.saveWebArchive(basename, autoname, callback);
    }

    @Override
    public void stopLoading() {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    stopLoading();
                }
            });
            return;
        }

        mAwContents.stopLoading();
    }

    @Override
    public void reload() {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    reload();
                }
            });
            return;
        }
        mAwContents.reload();
    }

    @Override
    public boolean canGoBack() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            Boolean ret = runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return canGoBack();
                }
            });
            return ret;
        }
        return mAwContents.canGoBack();
    }

    @Override
    public void goBack() {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    goBack();
                }
            });
            return;
        }
        mAwContents.goBack();
    }

    @Override
    public boolean canGoForward() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            Boolean ret = runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return canGoForward();
                }
            });
            return ret;
        }
        return mAwContents.canGoForward();
    }

    @Override
    public void goForward() {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    goForward();
                }
            });
            return;
        }
        mAwContents.goForward();
    }

    @Override
    public boolean canGoBackOrForward(final int steps) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            Boolean ret = runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return canGoBackOrForward(steps);
                }
            });
            return ret;
        }
        return mAwContents.canGoBackOrForward(steps);
    }

    @Override
    public void goBackOrForward(final int steps) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    goBackOrForward(steps);
                }
            });
            return;
        }
        mAwContents.goBackOrForward(steps);
    }

    @Override
    public boolean isPrivateBrowsingEnabled() {
        // Not supported in this WebView implementation.
        return false;
    }

    @Override
    public boolean pageUp(final boolean top) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            Boolean ret = runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return pageUp(top);
                }
            });
            return ret;
        }
        return mAwContents.pageUp(top);
    }

    @Override
    public boolean pageDown(final boolean bottom) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            Boolean ret = runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return pageDown(bottom);
                }
            });
            return ret;
        }
        return mAwContents.pageDown(bottom);
    }

    @Override
    public void insertVisualStateCallback(
            final long requestId, final VisualStateCallback callback) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    insertVisualStateCallback(requestId, callback);
                }
            });
            return;
        }
        mAwContents.insertVisualStateCallback(
                requestId, new AwContents.VisualStateCallback() {
                        @Override
                        public void onComplete(long requestId) {
                            callback.onComplete(requestId);
                        }
                });
    }

    @Override
    public void clearView() {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    clearView();
                }
            });
            return;
        }
        mAwContents.clearView();
    }

    @Override
    public Picture capturePicture() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            Picture ret = runOnUiThreadBlocking(new Callable<Picture>() {
                @Override
                public Picture call() {
                    return capturePicture();
                }
            });
            return ret;
        }
        return mAwContents.capturePicture();
    }

    @Override
    public float getScale() {
        // No checkThread() as it is mostly thread safe (workaround for b/10652991).
        mFactory.startYourEngines(true);
        return mAwContents.getScale();
    }

    @Override
    public void setInitialScale(final int scaleInPercent) {
        // No checkThread() as it is thread safe
        mWebSettings.getAwSettings().setInitialPageScale(scaleInPercent);
    }

    @Override
    public void invokeZoomPicker() {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    invokeZoomPicker();
                }
            });
            return;
        }
        mAwContents.invokeZoomPicker();
    }

    @Override
    public WebView.HitTestResult getHitTestResult() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            WebView.HitTestResult ret =
                    runOnUiThreadBlocking(new Callable<WebView.HitTestResult>() {
                        @Override
                        public WebView.HitTestResult call() {
                            return getHitTestResult();
                        }
                    });
            return ret;
        }
        AwContents.HitTestData data = mAwContents.getLastHitTestResult();
        mHitTestResult.setType(data.hitTestResultType);
        mHitTestResult.setExtra(data.hitTestResultExtraData);
        return mHitTestResult;
    }

    @Override
    public void requestFocusNodeHref(final Message hrefMsg) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    requestFocusNodeHref(hrefMsg);
                }
            });
            return;
        }
        mAwContents.requestFocusNodeHref(hrefMsg);
    }

    @Override
    public void requestImageRef(final Message msg) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    requestImageRef(msg);
                }
            });
            return;
        }
        mAwContents.requestImageRef(msg);
    }

    @Override
    public String getUrl() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            String ret = runOnUiThreadBlocking(new Callable<String>() {
                @Override
                public String call() {
                    return getUrl();
                }
            });
            return ret;
        }
        return mAwContents.getUrl();
    }

    @Override
    public String getOriginalUrl() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            String ret = runOnUiThreadBlocking(new Callable<String>() {
                @Override
                public String call() {
                    return getOriginalUrl();
                }
            });
            return ret;
        }
        return mAwContents.getOriginalUrl();
    }

    @Override
    public String getTitle() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            String ret = runOnUiThreadBlocking(new Callable<String>() {
                @Override
                public String call() {
                    return getTitle();
                }
            });
            return ret;
        }
        return mAwContents.getTitle();
    }

    @Override
    public Bitmap getFavicon() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            Bitmap ret = runOnUiThreadBlocking(new Callable<Bitmap>() {
                @Override
                public Bitmap call() {
                    return getFavicon();
                }
            });
            return ret;
        }
        return mAwContents.getFavicon();
    }

    @Override
    public String getTouchIconUrl() {
        // Intentional no-op: hidden method on WebView.
        return null;
    }

    @Override
    public int getProgress() {
        if (mAwContents == null) return 100;
        // No checkThread() because the value is cached java side (workaround for b/10533304).
        return mAwContents.getMostRecentProgress();
    }

    @Override
    public int getContentHeight() {
        if (mAwContents == null) return 0;
        // No checkThread() as it is mostly thread safe (workaround for b/10594869).
        return mAwContents.getContentHeightCss();
    }

    @Override
    public int getContentWidth() {
        if (mAwContents == null) return 0;
        // No checkThread() as it is mostly thread safe (workaround for b/10594869).
        return mAwContents.getContentWidthCss();
    }

    @Override
    public void pauseTimers() {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    pauseTimers();
                }
            });
            return;
        }
        mAwContents.pauseTimers();
    }

    @Override
    public void resumeTimers() {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    resumeTimers();
                }
            });
            return;
        }
        mAwContents.resumeTimers();
    }

    @Override
    public void onPause() {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    onPause();
                }
            });
            return;
        }
        mAwContents.onPause();
    }

    @Override
    public void onResume() {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    onResume();
                }
            });
            return;
        }
        mAwContents.onResume();
    }

    @Override
    public boolean isPaused() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            Boolean ret = runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return isPaused();
                }
            });
            return ret;
        }
        return mAwContents.isPaused();
    }

    @Override
    public void freeMemory() {
        // Intentional no-op. Memory is managed automatically by Chromium.
    }

    @Override
    public void clearCache(final boolean includeDiskFiles) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    clearCache(includeDiskFiles);
                }
            });
            return;
        }
        mAwContents.clearCache(includeDiskFiles);
    }

    /**
     * This is a poorly named method, but we keep it for historical reasons.
     */
    @Override
    public void clearFormData() {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    clearFormData();
                }
            });
            return;
        }
        mAwContents.hideAutofillPopup();
    }

    @Override
    public void clearHistory() {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    clearHistory();
                }
            });
            return;
        }
        mAwContents.clearHistory();
    }

    @Override
    public void clearSslPreferences() {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    clearSslPreferences();
                }
            });
            return;
        }
        mAwContents.clearSslPreferences();
    }

    @Override
    public WebBackForwardList copyBackForwardList() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            WebBackForwardList ret = runOnUiThreadBlocking(new Callable<WebBackForwardList>() {
                @Override
                public WebBackForwardList call() {
                    return copyBackForwardList();
                }
            });
            return ret;
        }
        // mAwContents.getNavigationHistory() can be null here if mAwContents has been destroyed,
        // and we do not handle passing null to the WebBackForwardListChromium constructor.
        NavigationHistory navHistory = mAwContents.getNavigationHistory();
        if (navHistory == null) navHistory = new NavigationHistory();
        return new WebBackForwardListChromium(navHistory);
    }

    @Override
    public void setFindListener(WebView.FindListener listener) {
        mContentsClientAdapter.setFindListener(listener);
    }

    @Override
    public void findNext(final boolean forwards) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    findNext(forwards);
                }
            });
            return;
        }
        mAwContents.findNext(forwards);
    }

    @Override
    public int findAll(final String searchString) {
        findAllAsync(searchString);
        return 0;
    }

    @Override
    public void findAllAsync(final String searchString) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    findAllAsync(searchString);
                }
            });
            return;
        }
        mAwContents.findAllAsync(searchString);
    }

    @SuppressFBWarnings("RCN_REDUNDANT_NULLCHECK_OF_NONNULL_VALUE")
    @Override
    public boolean showFindDialog(final String text, final boolean showIme) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            return false;
        }
        if (mWebView.getParent() == null) {
            return false;
        }

        FindActionModeCallback findAction = new FindActionModeCallback(mContext);
        if (findAction == null) {
            return false;
        }

        mWebView.startActionMode(findAction);
        findAction.setWebView(mWebView);
        if (showIme) {
            findAction.showSoftInput();
        }

        if (text != null) {
            findAction.setText(text);
            findAction.findAll();
        }

        return true;
    }

    @Override
    public void notifyFindDialogDismissed() {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    notifyFindDialogDismissed();
                }
            });
            return;
        }
        clearMatches();
    }

    @Override
    public void clearMatches() {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    clearMatches();
                }
            });
            return;
        }
        mAwContents.clearMatches();
    }

    @Override
    public void documentHasImages(final Message response) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    documentHasImages(response);
                }
            });
            return;
        }
        mAwContents.documentHasImages(response);
    }

    @Override
    public void setWebViewClient(WebViewClient client) {
        mContentsClientAdapter.setWebViewClient(client);
    }

    @Override
    public void setDownloadListener(DownloadListener listener) {
        mContentsClientAdapter.setDownloadListener(listener);
    }

    @Override
    public void setWebChromeClient(WebChromeClient client) {
        mWebSettings.getAwSettings().setFullscreenSupported(doesSupportFullscreen(client));
        mContentsClientAdapter.setWebChromeClient(client);
    }

    /**
     * Returns true if the supplied {@link WebChromeClient} supports fullscreen.
     *
     * <p>For fullscreen support, implementations of {@link WebChromeClient#onShowCustomView}
     * and {@link WebChromeClient#onHideCustomView()} are required.
     */
    private boolean doesSupportFullscreen(WebChromeClient client) {
        if (client == null) {
            return false;
        }
        Class<?> clientClass = client.getClass();
        boolean foundShowMethod = false;
        boolean foundHideMethod = false;
        while (clientClass != WebChromeClient.class && (!foundShowMethod || !foundHideMethod)) {
            if (!foundShowMethod) {
                try {
                    clientClass.getDeclaredMethod(
                            "onShowCustomView", View.class, CustomViewCallback.class);
                    foundShowMethod = true;
                } catch (NoSuchMethodException e) {
                    // Intentionally empty.
                }
            }

            if (!foundHideMethod) {
                try {
                    clientClass.getDeclaredMethod("onHideCustomView");
                    foundHideMethod = true;
                } catch (NoSuchMethodException e) {
                    // Intentionally empty.
                }
            }
            clientClass = clientClass.getSuperclass();
        }
        return foundShowMethod && foundHideMethod;
    }

    @Override
    @SuppressWarnings("deprecation")
    public void setPictureListener(final WebView.PictureListener listener) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    setPictureListener(listener);
                }
            });
            return;
        }
        mContentsClientAdapter.setPictureListener(listener);
        mAwContents.enableOnNewPicture(
                listener != null, mAppTargetSdkVersion >= Build.VERSION_CODES.JELLY_BEAN_MR2);
    }

    @Override
    public void addJavascriptInterface(final Object obj, final String interfaceName) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    addJavascriptInterface(obj, interfaceName);
                }
            });
            return;
        }
        mAwContents.addJavascriptInterface(obj, interfaceName);
    }

    @Override
    public void removeJavascriptInterface(final String interfaceName) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    removeJavascriptInterface(interfaceName);
                }
            });
            return;
        }
        mAwContents.removeJavascriptInterface(interfaceName);
    }

    @Override
    public WebMessagePort[] createWebMessageChannel() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            WebMessagePort[] ret = runOnUiThreadBlocking(new Callable<WebMessagePort[]>() {
                @Override
                public WebMessagePort[] call() {
                    return createWebMessageChannel();
                }
            });
            return ret;
        }
        return WebMessagePortAdapter.fromAwMessagePorts(mAwContents.createMessageChannel());
    }

    @Override
    @TargetApi(Build.VERSION_CODES.M)
    public void postMessageToMainFrame(final WebMessage message, final Uri targetOrigin) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    postMessageToMainFrame(message, targetOrigin);
                }
            });
            return;
        }
        mAwContents.postMessageToFrame(null, message.getData(), targetOrigin.toString(),
                WebMessagePortAdapter.toAwMessagePorts(message.getPorts()));
    }

    @Override
    public WebSettings getSettings() {
        return mWebSettings;
    }

    @Override
    public void setMapTrackballToArrowKeys(boolean setMap) {
        // This is a deprecated API: intentional no-op.
    }

    @Override
    public void flingScroll(final int vx, final int vy) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    flingScroll(vx, vy);
                }
            });
            return;
        }
        mAwContents.flingScroll(vx, vy);
    }

    @Override
    public View getZoomControls() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            return null;
        }

        // This was deprecated in 2009 and hidden in JB MR1, so just provide the minimum needed
        // to stop very out-dated applications from crashing.
        Log.w(TAG, "WebView doesn't support getZoomControls");
        return mAwContents.getSettings().supportZoom() ? new View(mContext) : null;
    }

    @Override
    public boolean canZoomIn() {
        if (checkNeedsPost()) {
            return false;
        }
        return mAwContents.canZoomIn();
    }

    @Override
    public boolean canZoomOut() {
        if (checkNeedsPost()) {
            return false;
        }
        return mAwContents.canZoomOut();
    }

    @Override
    public boolean zoomIn() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            boolean ret = runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return zoomIn();
                }
            });
            return ret;
        }
        return mAwContents.zoomIn();
    }

    @Override
    public boolean zoomOut() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            boolean ret = runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return zoomOut();
                }
            });
            return ret;
        }
        return mAwContents.zoomOut();
    }

    // TODO(paulmiller) Return void for consistency with AwContents.zoomBy and WebView.zoomBy -
    // tricky because frameworks WebViewProvider.zoomBy must change simultaneously
    @Override
    public boolean zoomBy(float factor) {
        mFactory.startYourEngines(true);
        // This is an L API and therefore we can enforce stricter threading constraints.
        checkThread();
        mAwContents.zoomBy(factor);
        return true;
    }

    @Override
    public void dumpViewHierarchyWithProperties(BufferedWriter out, int level) {
        // Intentional no-op
    }

    @Override
    public View findHierarchyView(String className, int hashCode) {
        // Intentional no-op
        return null;
    }

    // WebViewProvider glue methods ---------------------------------------------------------------

    @Override
    // This needs to be kept thread safe!
    public WebViewProvider.ViewDelegate getViewDelegate() {
        return this;
    }

    @Override
    // This needs to be kept thread safe!
    public WebViewProvider.ScrollDelegate getScrollDelegate() {
        return this;
    }

    // WebViewProvider.ViewDelegate implementation ------------------------------------------------

    // TODO: remove from WebViewProvider and use default implementation from
    // ViewGroup.
    @Override
    public boolean shouldDelayChildPressedState() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret = runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return shouldDelayChildPressedState();
                }
            });
            return ret;
        }
        return true;
    }

    @Override
    public AccessibilityNodeProvider getAccessibilityNodeProvider() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            AccessibilityNodeProvider ret =
                    runOnUiThreadBlocking(new Callable<AccessibilityNodeProvider>() {
                        @Override
                        public AccessibilityNodeProvider call() {
                            return getAccessibilityNodeProvider();
                        }
                    });
            return ret;
        }
        return mAwContents.getAccessibilityNodeProvider();
    }

    @TargetApi(Build.VERSION_CODES.M)
    @Override
    public void onProvideVirtualStructure(final ViewStructure structure) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            runVoidTaskOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    onProvideVirtualStructure(structure);
                }
            });
            return;
        }
        structure.setChildCount(1);
        final ViewStructure viewRoot = structure.asyncNewChild(0);
        mAwContents.requestAccessibilitySnapshot(new AccessibilitySnapshotCallback() {
            @Override
            public void onAccessibilitySnapshot(AccessibilitySnapshotNode root) {
                viewRoot.setHint(AwContentsStatics.getProductVersion());
                if (root == null) {
                    viewRoot.setClassName("android.webkit.WebView");
                    viewRoot.asyncCommit();
                    return;
                }
                createAssistStructure(viewRoot, root, 0, 0);
            }
        });
    }

    // When creating the Assist structure, the left and top are relative to the parent node, and
    // scroll offsets are not needed.
    @TargetApi(Build.VERSION_CODES.M)
    private void createAssistStructure(ViewStructure viewNode, AccessibilitySnapshotNode node,
            int parentX, int parentY) {
        viewNode.setClassName(node.className);
        if (node.hasSelection) {
            viewNode.setText(node.text, node.startSelection, node.endSelection);
        } else {
            viewNode.setText(node.text);
        }
        // Do not pass scroll information.
        viewNode.setDimens(node.x - parentX, node.y - parentY, 0, 0, node.width, node.height);
        viewNode.setChildCount(node.children.size());
        if (node.hasStyle) {
            int style = (node.bold ? ViewNode.TEXT_STYLE_BOLD : 0)
                    | (node.italic ? ViewNode.TEXT_STYLE_ITALIC : 0)
                    | (node.underline ? ViewNode.TEXT_STYLE_UNDERLINE : 0)
                    | (node.lineThrough ? ViewNode.TEXT_STYLE_STRIKE_THRU : 0);
            viewNode.setTextStyle(node.textSize, node.color, node.bgcolor, style);
        }
        final Iterator<AccessibilitySnapshotNode> children = node.children.listIterator();
        int i = 0;
        while (children.hasNext()) {
            createAssistStructure(viewNode.asyncNewChild(i++), children.next(), node.x, node.y);
        }
        viewNode.asyncCommit();
    }

    @Override
    public void onInitializeAccessibilityNodeInfo(final AccessibilityNodeInfo info) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            runVoidTaskOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    onInitializeAccessibilityNodeInfo(info);
                }
            });
            return;
        }
        mAwContents.onInitializeAccessibilityNodeInfo(info);
    }

    @Override
    public void onInitializeAccessibilityEvent(final AccessibilityEvent event) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            runVoidTaskOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    onInitializeAccessibilityEvent(event);
                }
            });
            return;
        }
        mAwContents.onInitializeAccessibilityEvent(event);
    }

    @Override
    public boolean performAccessibilityAction(final int action, final Bundle arguments) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret = runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return performAccessibilityAction(action, arguments);
                }
            });
            return ret;
        }
        if (mAwContents.supportsAccessibilityAction(action)) {
            return mAwContents.performAccessibilityAction(action, arguments);
        }
        return mWebViewPrivate.super_performAccessibilityAction(action, arguments);
    }

    @Override
    public void setOverScrollMode(final int mode) {
        // This gets called from the android.view.View c'tor that WebView inherits from. This
        // causes the method to be called when mAwContents == null.
        // It's safe to ignore these calls however since AwContents will read the current value of
        // this setting when it's created.
        if (mAwContents == null) return;

        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    setOverScrollMode(mode);
                }
            });
            return;
        }
        mAwContents.setOverScrollMode(mode);
    }

    @Override
    public void setScrollBarStyle(final int style) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    setScrollBarStyle(style);
                }
            });
            return;
        }
        mAwContents.setScrollBarStyle(style);
    }

    @Override
    public void onDrawVerticalScrollBar(final Canvas canvas, final Drawable scrollBar, final int l,
            final int t, final int r, final int b) {
        // WebViewClassic was overriding this method to handle rubberband over-scroll. Since
        // WebViewChromium doesn't support that the vanilla implementation of this method can be
        // used.
        mWebViewPrivate.super_onDrawVerticalScrollBar(canvas, scrollBar, l, t, r, b);
    }

    @Override
    public void onOverScrolled(final int scrollX, final int scrollY,
            final boolean clampedX, final boolean clampedY) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    onOverScrolled(scrollX, scrollY, clampedX, clampedY);
                }
            });
            return;
        }
        mAwContents.onContainerViewOverScrolled(scrollX, scrollY, clampedX, clampedY);
    }

    @Override
    public void onWindowVisibilityChanged(final int visibility) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    onWindowVisibilityChanged(visibility);
                }
            });
            return;
        }
        mAwContents.onWindowVisibilityChanged(visibility);
    }

    @Override
    @SuppressLint("DrawAllocation")
    public void onDraw(final Canvas canvas) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            runVoidTaskOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    onDraw(canvas);
                }
            });
            return;
        }
        mAwContents.onDraw(canvas);
    }

    @Override
    public void setLayoutParams(final ViewGroup.LayoutParams layoutParams) {
        // This API is our strongest signal from the View system that this
        // WebView is going to be bound to a View hierarchy and so at this
        // point we must bind Chromium's UI thread to the current thread.
        mFactory.startYourEngines(false);
        checkThread();
        mWebViewPrivate.super_setLayoutParams(layoutParams);
        if (checkNeedsPost()) {
            runVoidTaskOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    mAwContents.setLayoutParams(layoutParams);
                }
            });
            return;
        }
        mAwContents.setLayoutParams(layoutParams);
    }

    // Overrides WebViewProvider.ViewDelegate.onActivityResult (not in system api jar yet).
    // crbug.com/543272.
    public void onActivityResult(final int requestCode, final int resultCode, final Intent data) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    onActivityResult(requestCode, resultCode, data);
                }
            });
            return;
        }
        mAwContents.onActivityResult(requestCode, resultCode, data);
    }

    @Override
    public boolean performLongClick() {
        // Return false unless the WebView is attached to a View with a parent
        return mWebView.getParent() != null ? mWebViewPrivate.super_performLongClick() : false;
    }

    @Override
    public void onConfigurationChanged(final Configuration newConfig) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    onConfigurationChanged(newConfig);
                }
            });
            return;
        }
        mAwContents.onConfigurationChanged(newConfig);
    }

    @Override
    public InputConnection onCreateInputConnection(final EditorInfo outAttrs) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            return null;
        }
        return mAwContents.onCreateInputConnection(outAttrs);
    }

    @Override
    public boolean onKeyMultiple(final int keyCode, final int repeatCount, final KeyEvent event) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret = runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return onKeyMultiple(keyCode, repeatCount, event);
                }
            });
            return ret;
        }
        return false;
    }

    @Override
    public boolean onKeyDown(final int keyCode, final KeyEvent event) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret = runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return onKeyDown(keyCode, event);
                }
            });
            return ret;
        }
        return false;
    }

    @Override
    public boolean onKeyUp(final int keyCode, final KeyEvent event) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret = runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return onKeyUp(keyCode, event);
                }
            });
            return ret;
        }
        return mAwContents.onKeyUp(keyCode, event);
    }

    @Override
    public void onAttachedToWindow() {
        // This API is our strongest signal from the View system that this
        // WebView is going to be bound to a View hierarchy and so at this
        // point we must bind Chromium's UI thread to the current thread.
        mFactory.startYourEngines(false);
        checkThread();
        mAwContents.onAttachedToWindow();
    }

    @Override
    public void onDetachedFromWindow() {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    onDetachedFromWindow();
                }
            });
            return;
        }

        mAwContents.onDetachedFromWindow();
    }

    @Override
    public void onVisibilityChanged(final View changedView, final int visibility) {
        // The AwContents will find out the container view visibility before the first draw so we
        // can safely ignore onVisibilityChanged callbacks that happen before init().
        if (mAwContents == null) return;

        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    onVisibilityChanged(changedView, visibility);
                }
            });
            return;
        }
        mAwContents.onVisibilityChanged(changedView, visibility);
    }

    @Override
    public void onWindowFocusChanged(final boolean hasWindowFocus) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    onWindowFocusChanged(hasWindowFocus);
                }
            });
            return;
        }
        mAwContents.onWindowFocusChanged(hasWindowFocus);
    }

    @Override
    public void onFocusChanged(
            final boolean focused, final int direction, final Rect previouslyFocusedRect) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    onFocusChanged(focused, direction, previouslyFocusedRect);
                }
            });
            return;
        }
        mAwContents.onFocusChanged(focused, direction, previouslyFocusedRect);
    }

    @Override
    public boolean setFrame(final int left, final int top, final int right, final int bottom) {
        return mWebViewPrivate.super_setFrame(left, top, right, bottom);
    }

    @Override
    public void onSizeChanged(final int w, final int h, final int ow, final int oh) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    onSizeChanged(w, h, ow, oh);
                }
            });
            return;
        }
        mAwContents.onSizeChanged(w, h, ow, oh);
    }

    @Override
    public void onScrollChanged(final int l, final int t, final int oldl, final int oldt) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(new Runnable() {
                @Override
                public void run() {
                    onScrollChanged(l, t, oldl, oldt);
                }
            });
            return;
        }
        mAwContents.onContainerViewScrollChanged(l, t, oldl, oldt);
    }

    @Override
    public boolean dispatchKeyEvent(final KeyEvent event) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret = runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return dispatchKeyEvent(event);
                }
            });
            return ret;
        }
        return mAwContents.dispatchKeyEvent(event);
    }

    @Override
    public boolean onTouchEvent(final MotionEvent ev) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret = runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return onTouchEvent(ev);
                }
            });
            return ret;
        }
        return mAwContents.onTouchEvent(ev);
    }

    @Override
    public boolean onHoverEvent(final MotionEvent event) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret = runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return onHoverEvent(event);
                }
            });
            return ret;
        }
        return mAwContents.onHoverEvent(event);
    }

    @Override
    public boolean onGenericMotionEvent(final MotionEvent event) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret = runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return onGenericMotionEvent(event);
                }
            });
            return ret;
        }
        return mAwContents.onGenericMotionEvent(event);
    }

    @Override
    public boolean onTrackballEvent(MotionEvent ev) {
        // Trackball event not handled, which eventually gets converted to DPAD keyevents
        return false;
    }

    @Override
    public boolean requestFocus(final int direction, final Rect previouslyFocusedRect) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret = runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return requestFocus(direction, previouslyFocusedRect);
                }
            });
            return ret;
        }
        mAwContents.requestFocus();
        return mWebViewPrivate.super_requestFocus(direction, previouslyFocusedRect);
    }

    @Override
    @SuppressLint("DrawAllocation")
    public void onMeasure(final int widthMeasureSpec, final int heightMeasureSpec) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            runVoidTaskOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    onMeasure(widthMeasureSpec, heightMeasureSpec);
                }
            });
            return;
        }
        mAwContents.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    public boolean requestChildRectangleOnScreen(
            final View child, final Rect rect, final boolean immediate) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret = runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return requestChildRectangleOnScreen(child, rect, immediate);
                }
            });
            return ret;
        }
        return mAwContents.requestChildRectangleOnScreen(child, rect, immediate);
    }

    @Override
    public void setBackgroundColor(final int color) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            ThreadUtils.postOnUiThread(new Runnable() {
                @Override
                public void run() {
                    setBackgroundColor(color);
                }
            });
            return;
        }
        mAwContents.setBackgroundColor(color);
    }

    @Override
    public void setLayerType(final int layerType, final Paint paint) {
        // This can be called from WebView constructor in which case mAwContents
        // is still null. We set the layer type in initForReal in that case.
        if (mAwContents == null) return;
        if (checkNeedsPost()) {
            ThreadUtils.postOnUiThread(new Runnable() {
                @Override
                public void run() {
                    setLayerType(layerType, paint);
                }
            });
            return;
        }
        mAwContents.setLayerType(layerType, paint);
    }

    // Overrides method added to WebViewProvider.ViewDelegate interface
    // (not called in M and below)
    public Handler getHandler(Handler originalHandler) {
        return originalHandler;
    }

    // Overrides method added to WebViewProvider.ViewDelegate interface
    // (not called in M and below)
    public View findFocus(View originalFocusedView) {
        return originalFocusedView;
    }

    // Remove from superclass
    public void preDispatchDraw(Canvas canvas) {
        // TODO(leandrogracia): remove this method from WebViewProvider if we think
        // we won't need it again.
    }

    public void onStartTemporaryDetach() {
        mAwContents.onStartTemporaryDetach();
    }

    public void onFinishTemporaryDetach() {
        mAwContents.onFinishTemporaryDetach();
    }

    // WebViewProvider.ScrollDelegate implementation ----------------------------------------------

    @Override
    public int computeHorizontalScrollRange() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            int ret = runOnUiThreadBlocking(new Callable<Integer>() {
                @Override
                public Integer call() {
                    return computeHorizontalScrollRange();
                }
            });
            return ret;
        }
        return mAwContents.computeHorizontalScrollRange();
    }

    @Override
    public int computeHorizontalScrollOffset() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            int ret = runOnUiThreadBlocking(new Callable<Integer>() {
                @Override
                public Integer call() {
                    return computeHorizontalScrollOffset();
                }
            });
            return ret;
        }
        return mAwContents.computeHorizontalScrollOffset();
    }

    @Override
    public int computeVerticalScrollRange() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            int ret = runOnUiThreadBlocking(new Callable<Integer>() {
                @Override
                public Integer call() {
                    return computeVerticalScrollRange();
                }
            });
            return ret;
        }
        return mAwContents.computeVerticalScrollRange();
    }

    @Override
    public int computeVerticalScrollOffset() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            int ret = runOnUiThreadBlocking(new Callable<Integer>() {
                @Override
                public Integer call() {
                    return computeVerticalScrollOffset();
                }
            });
            return ret;
        }
        return mAwContents.computeVerticalScrollOffset();
    }

    @Override
    public int computeVerticalScrollExtent() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            int ret = runOnUiThreadBlocking(new Callable<Integer>() {
                @Override
                public Integer call() {
                    return computeVerticalScrollExtent();
                }
            });
            return ret;
        }
        return mAwContents.computeVerticalScrollExtent();
    }

    @Override
    public void computeScroll() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            runVoidTaskOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    computeScroll();
                }
            });
            return;
        }
        mAwContents.computeScroll();
    }

    @Override
    public PrintDocumentAdapter createPrintDocumentAdapter(String documentName) {
        checkThread();
        return new AwPrintDocumentAdapter(mAwContents.getPdfExporter(), documentName);
    }

    // AwContents.NativeGLDelegate implementation --------------------------------------
    private class WebViewNativeGLDelegate implements AwContents.NativeGLDelegate {
        @Override
        public boolean requestDrawGL(Canvas canvas, boolean waitForCompletion, View containerView) {
            if (mGLfunctor == null) {
                mGLfunctor = new DrawGLFunctor(
                        mAwContents.getAwDrawGLViewContext(), mFactory.getWebViewDelegate());
            }
            return mGLfunctor.requestDrawGL(canvas, containerView, waitForCompletion);
        }

        @Override
        public void detachGLFunctor() {
            if (mGLfunctor != null) {
                mGLfunctor.detach();
            }
        }
    }

    // AwContents.InternalAccessDelegate implementation --------------------------------------
    private class InternalAccessAdapter implements AwContents.InternalAccessDelegate {
        @Override
        public boolean super_onKeyUp(int arg0, KeyEvent arg1) {
            // Intentional no-op
            return false;
        }

        @Override
        public boolean super_dispatchKeyEventPreIme(KeyEvent arg0) {
            return false;
        }

        @Override
        public boolean super_dispatchKeyEvent(KeyEvent event) {
            return mWebViewPrivate.super_dispatchKeyEvent(event);
        }

        @Override
        public boolean super_onGenericMotionEvent(MotionEvent arg0) {
            return mWebViewPrivate.super_onGenericMotionEvent(arg0);
        }

        @Override
        public void super_onConfigurationChanged(Configuration arg0) {
            // Intentional no-op
        }

        @Override
        public int super_getScrollBarStyle() {
            return mWebViewPrivate.super_getScrollBarStyle();
        }

        @Override
        public void super_startActivityForResult(Intent intent, int requestCode) {
            // TODO(hush): Use mWebViewPrivate.super_startActivityForResult
            // after N release. crbug.com/543272.
            try {
                Method startActivityForResultMethod =
                        View.class.getMethod("startActivityForResult", Intent.class, int.class);
                startActivityForResultMethod.invoke(mWebView, intent, requestCode);
            } catch (Exception e) {
                throw new RuntimeException("Invalid reflection", e);
            }
        }

        @Override
        public boolean awakenScrollBars() {
            mWebViewPrivate.awakenScrollBars(0);
            // TODO: modify the WebView.PrivateAccess to provide a return value.
            return true;
        }

        @Override
        public boolean super_awakenScrollBars(int arg0, boolean arg1) {
            return false;
        }

        @Override
        public void onScrollChanged(int l, int t, int oldl, int oldt) {
            // Intentional no-op.
            // Chromium calls this directly to trigger accessibility events. That isn't needed
            // for WebView since super_scrollTo invokes onScrollChanged for us.
        }

        @Override
        public void overScrollBy(int deltaX, int deltaY, int scrollX, int scrollY, int scrollRangeX,
                int scrollRangeY, int maxOverScrollX, int maxOverScrollY, boolean isTouchEvent) {
            mWebViewPrivate.overScrollBy(deltaX, deltaY, scrollX, scrollY, scrollRangeX,
                    scrollRangeY, maxOverScrollX, maxOverScrollY, isTouchEvent);
        }

        @Override
        public void super_scrollTo(int scrollX, int scrollY) {
            mWebViewPrivate.super_scrollTo(scrollX, scrollY);
        }

        @Override
        public void setMeasuredDimension(int measuredWidth, int measuredHeight) {
            mWebViewPrivate.setMeasuredDimension(measuredWidth, measuredHeight);
        }

        // @Override
        public boolean super_onHoverEvent(MotionEvent event) {
            return mWebViewPrivate.super_onHoverEvent(event);
        }
    }

    // Implements SmartClipProvider
    @Override
    public void extractSmartClipData(int x, int y, int width, int height) {
        checkThread();
        mAwContents.extractSmartClipData(x, y, width, height);
    }

    // Implements SmartClipProvider
    @Override
    public void setSmartClipResultHandler(final Handler resultHandler) {
        checkThread();
        mAwContents.setSmartClipResultHandler(resultHandler);
    }
}

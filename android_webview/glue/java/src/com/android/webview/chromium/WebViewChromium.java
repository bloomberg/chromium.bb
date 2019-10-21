// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
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
import android.os.SystemClock;
import android.print.PrintDocumentAdapter;
import android.util.Log;
import android.util.SparseArray;
import android.view.DragEvent;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStructure;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeProvider;
import android.view.autofill.AutofillValue;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.textclassifier.TextClassifier;
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

import androidx.annotation.IntDef;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.AwPrintDocumentAdapter;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.gfx.AwDrawFnImpl;
import org.chromium.android_webview.renderer_priority.RendererPriority;
import org.chromium.base.BuildInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.CachedMetrics.EnumeratedHistogramSample;
import org.chromium.base.metrics.CachedMetrics.TimesHistogramSample;
import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.base.task.PostTask;
import org.chromium.components.autofill.AutofillProvider;
import org.chromium.components.embedder_support.application.ClassLoaderContextWrapperFactory;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.SmartClipProvider;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.io.BufferedWriter;
import java.io.File;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.Map;
import java.util.concurrent.Callable;

/**
 * This class is the delegate to which WebViewProxy forwards all API calls.
 *
 * Most of the actual functionality is implemented by AwContents (or WebContents within
 * it). This class also contains WebView-specific APIs that require the creation of other
 * adapters (otherwise org.chromium.content would depend on the webview.chromium package)
 * and a small set of no-op deprecated APIs.
 */
@SuppressWarnings("deprecation")
class WebViewChromium implements WebViewProvider, WebViewProvider.ScrollDelegate,
                                 WebViewProvider.ViewDelegate, SmartClipProvider {

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
    // The WebView wrapper for WebContents and required browser components.
    AwContents mAwContents;

    private final WebView.HitTestResult mHitTestResult;

    private final int mAppTargetSdkVersion;

    protected WebViewChromiumFactoryProvider mFactory;

    protected final SharedWebViewChromium mSharedWebViewChromium;

    private final boolean mShouldDisableThreadChecking;

    private static boolean sRecordWholeDocumentEnabledByApi;
    static void enableSlowWholeDocumentDraw() {
        sRecordWholeDocumentEnabledByApi = true;
    }

    // Used to record the UMA histogram WebView.WebViewApiCall. Since these value are persisted to
    // logs, they should never be renumbered nor reused.
    @IntDef({ApiCall.ADD_JAVASCRIPT_INTERFACE, ApiCall.AUTOFILL, ApiCall.CAN_GO_BACK,
            ApiCall.CAN_GO_BACK_OR_FORWARD, ApiCall.CAN_GO_FORWARD, ApiCall.CAN_ZOOM_IN,
            ApiCall.CAN_ZOOM_OUT, ApiCall.CAPTURE_PICTURE, ApiCall.CLEAR_CACHE,
            ApiCall.CLEAR_FORM_DATA, ApiCall.CLEAR_HISTORY, ApiCall.CLEAR_MATCHES,
            ApiCall.CLEAR_SSL_PREFERENCES, ApiCall.CLEAR_VIEW, ApiCall.COPY_BACK_FORWARD_LIST,
            ApiCall.CREATE_PRINT_DOCUMENT_ADAPTER, ApiCall.CREATE_WEBMESSAGE_CHANNEL,
            ApiCall.DOCUMENT_HAS_IMAGES, ApiCall.DOES_SUPPORT_FULLSCREEN,
            ApiCall.EVALUATE_JAVASCRIPT, ApiCall.EXTRACT_SMART_CLIP_DATA, ApiCall.FIND_NEXT,
            ApiCall.GET_CERTIFICATE, ApiCall.GET_CONTENT_HEIGHT, ApiCall.GET_CONTENT_WIDTH,
            ApiCall.GET_FAVICON, ApiCall.GET_HIT_TEST_RESULT,
            ApiCall.GET_HTTP_AUTH_USERNAME_PASSWORD, ApiCall.GET_ORIGINAL_URL, ApiCall.GET_PROGRESS,
            ApiCall.GET_SCALE, ApiCall.GET_SETTINGS, ApiCall.GET_TEXT_CLASSIFIER, ApiCall.GET_TITLE,
            ApiCall.GET_URL, ApiCall.GET_WEBCHROME_CLIENT, ApiCall.GET_WEBVIEW_CLIENT,
            ApiCall.GO_BACK, ApiCall.GO_BACK_OR_FORWARD, ApiCall.GO_FORWARD,
            ApiCall.INSERT_VISUAL_STATE_CALLBACK, ApiCall.INVOKE_ZOOM_PICKER, ApiCall.IS_PAUSED,
            ApiCall.IS_PRIVATE_BROWSING_ENABLED, ApiCall.LOAD_DATA, ApiCall.LOAD_DATA_WITH_BASE_URL,
            ApiCall.NOTIFY_FIND_DIALOG_DISMISSED, ApiCall.ON_PAUSE,
            ApiCall.ON_PROVIDE_AUTOFILL_VIRTUAL_STRUCTURE, ApiCall.ON_RESUME,
            ApiCall.OVERLAY_HORIZONTAL_SCROLLBAR, ApiCall.OVERLAY_VERTICAL_SCROLLBAR,
            ApiCall.PAGE_DOWN, ApiCall.PAGE_UP, ApiCall.PAUSE_TIMERS,
            ApiCall.POST_MESSAGE_TO_MAIN_FRAME, ApiCall.POST_URL, ApiCall.RELOAD,
            ApiCall.REMOVE_JAVASCRIPT_INTERFACE, ApiCall.REQUEST_FOCUS_NODE_HREF,
            ApiCall.REQUEST_IMAGE_REF, ApiCall.RESTORE_STATE, ApiCall.RESUME_TIMERS,
            ApiCall.SAVE_STATE, ApiCall.SET_DOWNLOAD_LISTENER, ApiCall.SET_FIND_LISTENER,
            ApiCall.SET_HORIZONTAL_SCROLLBAR_OVERLAY, ApiCall.SET_HTTP_AUTH_USERNAME_PASSWORD,
            ApiCall.SET_INITIAL_SCALE, ApiCall.SET_NETWORK_AVAILABLE, ApiCall.SET_PICTURE_LISTENER,
            ApiCall.SET_SMART_CLIP_RESULT_HANDLER, ApiCall.SET_TEXT_CLASSIFIER,
            ApiCall.SET_VERTICAL_SCROLLBAR_OVERLAY, ApiCall.SET_WEBCHROME_CLIENT,
            ApiCall.SET_WEBVIEW_CLIENT, ApiCall.SHOW_FIND_DIALOG, ApiCall.STOP_LOADING})
    @interface ApiCall {
        int ADD_JAVASCRIPT_INTERFACE = 0;
        int AUTOFILL = 1;
        int CAN_GO_BACK = 2;
        int CAN_GO_BACK_OR_FORWARD = 3;
        int CAN_GO_FORWARD = 4;
        int CAN_ZOOM_IN = 5;
        int CAN_ZOOM_OUT = 6;
        int CAPTURE_PICTURE = 7;
        int CLEAR_CACHE = 8;
        int CLEAR_FORM_DATA = 9;
        int CLEAR_HISTORY = 10;
        int CLEAR_MATCHES = 11;
        int CLEAR_SSL_PREFERENCES = 12;
        int CLEAR_VIEW = 13;
        int COPY_BACK_FORWARD_LIST = 14;
        int CREATE_PRINT_DOCUMENT_ADAPTER = 15;
        int CREATE_WEBMESSAGE_CHANNEL = 16;
        int DOCUMENT_HAS_IMAGES = 17;
        int DOES_SUPPORT_FULLSCREEN = 18;
        int EVALUATE_JAVASCRIPT = 19;
        int EXTRACT_SMART_CLIP_DATA = 20;
        int FIND_NEXT = 21;
        int GET_CERTIFICATE = 22;
        int GET_CONTENT_HEIGHT = 23;
        int GET_CONTENT_WIDTH = 24;
        int GET_FAVICON = 25;
        int GET_HIT_TEST_RESULT = 26;
        int GET_HTTP_AUTH_USERNAME_PASSWORD = 27;
        int GET_ORIGINAL_URL = 28;
        int GET_PROGRESS = 29;
        int GET_SCALE = 30;
        int GET_SETTINGS = 31;
        int GET_TEXT_CLASSIFIER = 32;
        int GET_TITLE = 33;
        int GET_URL = 34;
        int GET_WEBCHROME_CLIENT = 35;
        int GET_WEBVIEW_CLIENT = 36;
        int GO_BACK = 37;
        int GO_BACK_OR_FORWARD = 38;
        int GO_FORWARD = 39;
        int INSERT_VISUAL_STATE_CALLBACK = 40;
        int INVOKE_ZOOM_PICKER = 41;
        int IS_PAUSED = 42;
        int IS_PRIVATE_BROWSING_ENABLED = 43;
        int LOAD_DATA = 44;
        int LOAD_DATA_WITH_BASE_URL = 45;
        int NOTIFY_FIND_DIALOG_DISMISSED = 46;
        int ON_PAUSE = 47;
        int ON_PROVIDE_AUTOFILL_VIRTUAL_STRUCTURE = 48;
        int ON_RESUME = 49;
        int OVERLAY_HORIZONTAL_SCROLLBAR = 50;
        int OVERLAY_VERTICAL_SCROLLBAR = 51;
        int PAGE_DOWN = 52;
        int PAGE_UP = 53;
        int PAUSE_TIMERS = 54;
        int POST_MESSAGE_TO_MAIN_FRAME = 55;
        int POST_URL = 56;
        int RELOAD = 57;
        int REMOVE_JAVASCRIPT_INTERFACE = 58;
        int REQUEST_FOCUS_NODE_HREF = 59;
        int REQUEST_IMAGE_REF = 60;
        int RESTORE_STATE = 61;
        int RESUME_TIMERS = 62;
        int SAVE_STATE = 63;
        int SET_DOWNLOAD_LISTENER = 64;
        int SET_FIND_LISTENER = 65;
        int SET_HORIZONTAL_SCROLLBAR_OVERLAY = 66;
        int SET_HTTP_AUTH_USERNAME_PASSWORD = 67;
        int SET_INITIAL_SCALE = 68;
        int SET_NETWORK_AVAILABLE = 69;
        int SET_PICTURE_LISTENER = 70;
        int SET_SMART_CLIP_RESULT_HANDLER = 71;
        int SET_TEXT_CLASSIFIER = 72;
        int SET_VERTICAL_SCROLLBAR_OVERLAY = 73;
        int SET_WEBCHROME_CLIENT = 74;
        int SET_WEBVIEW_CLIENT = 75;
        int SHOW_FIND_DIALOG = 76;
        int STOP_LOADING = 77;
        int COUNT = 78;
    }

    private static final EnumeratedHistogramSample sWebViewApiCallSample =
            new EnumeratedHistogramSample("WebView.ApiCall", ApiCall.COUNT);

    // This does not touch any global / non-threadsafe state, but note that
    // init is ofter called right after and is NOT threadsafe.
    public WebViewChromium(WebViewChromiumFactoryProvider factory, WebView webView,
            WebView.PrivateAccess webViewPrivate, boolean shouldDisableThreadChecking) {
        try (ScopedSysTraceEvent e1 = ScopedSysTraceEvent.scoped("WebViewChromium.constructor")) {
            WebViewChromiumFactoryProvider.checkStorageIsNotDeviceProtected(webView.getContext());
            mWebView = webView;
            mWebViewPrivate = webViewPrivate;
            mHitTestResult = new WebView.HitTestResult();
            mContext = ClassLoaderContextWrapperFactory.get(mWebView.getContext());
            mAppTargetSdkVersion = mContext.getApplicationInfo().targetSdkVersion;
            mFactory = factory;
            mShouldDisableThreadChecking = shouldDisableThreadChecking;
            factory.getWebViewDelegate().addWebViewAssetPath(mWebView.getContext());
            mSharedWebViewChromium =
                    new SharedWebViewChromium(mFactory.getRunQueue(), mFactory.getAwInit());
        }
    }

    static void completeWindowCreation(WebView parent, WebView child) {
        AwContents parentContents = ((WebViewChromium) parent.getWebViewProvider()).mAwContents;
        AwContents childContents =
                child == null ? null : ((WebViewChromium) child.getWebViewProvider()).mAwContents;
        parentContents.supplyContentsForPopup(childContents);
    }

    // WebViewProvider methods --------------------------------------------------------------------

    @Override
    // BUG=6790250 |javaScriptInterfaces| was only ever used by the obsolete DumpRenderTree
    // so is ignored. TODO: remove it from WebViewProvider.
    public void init(final Map<String, Object> javaScriptInterfaces,
            final boolean privateBrowsing) {
        long startTime = SystemClock.elapsedRealtime();
        boolean isFirstWebViewInit = !mFactory.hasStarted();
        try (ScopedSysTraceEvent e1 = ScopedSysTraceEvent.scoped("WebViewChromium.init")) {
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
            final boolean areLegacyQuirksEnabled =
                    mAppTargetSdkVersion < Build.VERSION_CODES.KITKAT;
            final boolean allowEmptyDocumentPersistence =
                    mAppTargetSdkVersion <= Build.VERSION_CODES.M;
            final boolean allowGeolocationOnInsecureOrigins =
                    mAppTargetSdkVersion <= Build.VERSION_CODES.M;

            // https://crbug.com/698752
            final boolean doNotUpdateSelectionOnMutatingSelectionRange =
                    mAppTargetSdkVersion <= Build.VERSION_CODES.M;

            mContentsClientAdapter =
                    mFactory.createWebViewContentsClientAdapter(mWebView, mContext);
            try (ScopedSysTraceEvent e2 =
                            ScopedSysTraceEvent.scoped("WebViewChromium.ContentSettingsAdapter")) {
                mWebSettings = mFactory.createContentSettingsAdapter(new AwSettings(mContext,
                        isAccessFromFileURLsGrantedByDefault, areLegacyQuirksEnabled,
                        allowEmptyDocumentPersistence, allowGeolocationOnInsecureOrigins,
                        doNotUpdateSelectionOnMutatingSelectionRange));
            }

            if (mAppTargetSdkVersion < Build.VERSION_CODES.LOLLIPOP) {
                // Prior to Lollipop we always allowed third party cookies and mixed content.
                mWebSettings.setMixedContentMode(WebSettings.MIXED_CONTENT_ALWAYS_ALLOW);
                mWebSettings.setAcceptThirdPartyCookies(true);
                mWebSettings.getAwSettings().setZeroLayoutHeightDisablesViewportQuirk(true);
            }

            if (mAppTargetSdkVersion >= Build.VERSION_CODES.P) {
                mWebSettings.getAwSettings().setCSSHexAlphaColorEnabled(true);
                mWebSettings.getAwSettings().setScrollTopLeftInteropEnabled(true);
            }

            if (mShouldDisableThreadChecking) disableThreadChecking();

            mSharedWebViewChromium.init(mContentsClientAdapter);

            mFactory.addTask(new Runnable() {
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

        // If initialization hasn't been deferred, record a startup time histogram entry.
        if (mFactory.hasStarted()) {
            TimesHistogramSample histogram = new TimesHistogramSample(
                    "Android.WebView.Startup.CreationTime.Stage2.ProviderInit."
                    + (isFirstWebViewInit ? "Cold" : "Warm"));
            histogram.record(SystemClock.elapsedRealtime() - startTime);
        }
    }

    // This is a workaround for https://crbug.com/622151.
    // In HTC's email app, InputConnection.setComposingText() will call WebView.evaluateJavaScript,
    // and thread assertion will occur. We turn off WebView thread assertion for this app.
    private void disableThreadChecking() {
        try {
            Class<?> webViewClass = Class.forName("android.webkit.WebView");
            Field field = webViewClass.getDeclaredField("sEnforceThreadChecking");
            field.setAccessible(true);
            field.setBoolean(null, false);
            field.setAccessible(false);
        } catch (ClassNotFoundException | NoSuchFieldException | IllegalAccessException
                | IllegalArgumentException e) {
            Log.w(TAG, "Failed to disable thread checking.");
        }
    }

    private void initForReal() {
        try (ScopedSysTraceEvent e1 = ScopedSysTraceEvent.scoped("WebViewChromium.initForReal")) {
            AwContentsStatics.setRecordFullDocument(sRecordWholeDocumentEnabledByApi
                    || mAppTargetSdkVersion < Build.VERSION_CODES.LOLLIPOP);

            mAwContents = new AwContents(mFactory.getBrowserContextOnUiThread(), mWebView, mContext,
                    new InternalAccessAdapter(), new WebViewNativeDrawFunctorFactory(),
                    mContentsClientAdapter, mWebSettings.getAwSettings(),
                    new AwContents.DependencyFactory() {
                        @Override
                        public AutofillProvider createAutofillProvider(
                                Context context, ViewGroup containerView) {
                            return mFactory.createAutofillProvider(context, mWebView);
                        }
                    });
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

            mSharedWebViewChromium.initForReal(mAwContents);
        }
    }

    private RuntimeException createThreadException() {
        return new IllegalStateException(
                "Calling View methods on another thread than the UI thread.");
    }

    protected boolean checkNeedsPost() {
        return mSharedWebViewChromium.checkNeedsPost();
    }

    //  Intentionally not static, as no need to check thread on static methods
    private void checkThread() {
        if (!ThreadUtils.runningOnUiThread()) {
            final RuntimeException threadViolation = createThreadException();
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
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
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    setHorizontalScrollbarOverlay(overlay);
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.SET_HORIZONTAL_SCROLLBAR_OVERLAY);
        mAwContents.setHorizontalScrollbarOverlay(overlay);
    }

    @Override
    public void setVerticalScrollbarOverlay(final boolean overlay) {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    setVerticalScrollbarOverlay(overlay);
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.SET_VERTICAL_SCROLLBAR_OVERLAY);
        mAwContents.setVerticalScrollbarOverlay(overlay);
    }

    @Override
    public boolean overlayHorizontalScrollbar() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret = mFactory.runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return overlayHorizontalScrollbar();
                }
            });
            return ret;
        }
        sWebViewApiCallSample.record(ApiCall.OVERLAY_HORIZONTAL_SCROLLBAR);
        return mAwContents.overlayHorizontalScrollbar();
    }

    @Override
    public boolean overlayVerticalScrollbar() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret = mFactory.runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return overlayVerticalScrollbar();
                }
            });
            return ret;
        }
        sWebViewApiCallSample.record(ApiCall.OVERLAY_VERTICAL_SCROLLBAR);
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
            SslCertificate ret = mFactory.runOnUiThreadBlocking(new Callable<SslCertificate>() {
                @Override
                public SslCertificate call() {
                    return getCertificate();
                }
            });
            return ret;
        }
        sWebViewApiCallSample.record(ApiCall.GET_CERTIFICATE);
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
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    setHttpAuthUsernamePassword(host, realm, username, password);
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.SET_HTTP_AUTH_USERNAME_PASSWORD);
        ((WebViewDatabaseAdapter) mFactory.getWebViewDatabase(mContext))
                .setHttpAuthUsernamePassword(host, realm, username, password);
    }

    @Override
    public String[] getHttpAuthUsernamePassword(final String host, final String realm) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            String[] ret = mFactory.runOnUiThreadBlocking(new Callable<String[]>() {
                @Override
                public String[] call() {
                    return getHttpAuthUsernamePassword(host, realm);
                }
            });
            return ret;
        }
        sWebViewApiCallSample.record(ApiCall.GET_HTTP_AUTH_USERNAME_PASSWORD);
        return ((WebViewDatabaseAdapter) mFactory.getWebViewDatabase(mContext))
                .getHttpAuthUsernamePassword(host, realm);
    }

    @Override
    public void destroy() {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    destroy();
                }
            });
            return;
        }

        // Make sure that we do not trigger any callbacks after destruction
        setWebChromeClient(null);
        setWebViewClient(null);
        mContentsClientAdapter.setPictureListener(null, true);
        mContentsClientAdapter.setFindListener(null);
        mContentsClientAdapter.setDownloadListener(null);

        mAwContents.destroy();
    }

    @Override
    public void setNetworkAvailable(final boolean networkUp) {
        // Note that this purely toggles the JS navigator.online property.
        // It does not in affect chromium or network stack state in any way.
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    setNetworkAvailable(networkUp);
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.SET_NETWORK_AVAILABLE);
        mAwContents.setNetworkAvailable(networkUp);
    }

    @Override
    public WebBackForwardList saveState(final Bundle outState) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            WebBackForwardList ret =
                    mFactory.runOnUiThreadBlocking(new Callable<WebBackForwardList>() {
                        @Override
                        public WebBackForwardList call() {
                            return saveState(outState);
                        }
                    });
            return ret;
        }
        sWebViewApiCallSample.record(ApiCall.SAVE_STATE);
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
            WebBackForwardList ret =
                    mFactory.runOnUiThreadBlocking(new Callable<WebBackForwardList>() {
                        @Override
                        public WebBackForwardList call() {
                            return restoreState(inState);
                        }
                    });
            return ret;
        }
        sWebViewApiCallSample.record(ApiCall.RESTORE_STATE);
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
            mFactory.addTask(new Runnable() {
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
            mFactory.addTask(new Runnable() {
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
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    sWebViewApiCallSample.record(ApiCall.POST_URL);
                    mAwContents.postUrl(url, postData);
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.POST_URL);
        mAwContents.postUrl(url, postData);
    }

    @Override
    public void loadData(final String data, final String mimeType, final String encoding) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            // Disallowed in WebView API for apps targetting a new SDK
            assert mAppTargetSdkVersion < Build.VERSION_CODES.JELLY_BEAN_MR2;
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    sWebViewApiCallSample.record(ApiCall.LOAD_DATA);
                    mAwContents.loadData(data, mimeType, encoding);
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.LOAD_DATA);
        mAwContents.loadData(data, mimeType, encoding);
    }

    @Override
    public void loadDataWithBaseURL(final String baseUrl, final String data, final String mimeType,
            final String encoding, final String historyUrl) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            // Disallowed in WebView API for apps targetting a new SDK
            assert mAppTargetSdkVersion < Build.VERSION_CODES.JELLY_BEAN_MR2;
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    sWebViewApiCallSample.record(ApiCall.LOAD_DATA_WITH_BASE_URL);
                    mAwContents.loadDataWithBaseURL(baseUrl, data, mimeType, encoding, historyUrl);
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.LOAD_DATA_WITH_BASE_URL);
        mAwContents.loadDataWithBaseURL(baseUrl, data, mimeType, encoding, historyUrl);
    }

    @Override
    public void evaluateJavaScript(
            final String script, final ValueCallback<String> resultCallback) {
        if (mShouldDisableThreadChecking && checkNeedsPost()) {
            // This is a workaround for https://crbug.com/622151.
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    sWebViewApiCallSample.record(ApiCall.EVALUATE_JAVASCRIPT);
                    mAwContents.evaluateJavaScript(
                            script, CallbackConverter.fromValueCallback(resultCallback));
                }
            });
        } else {
            sWebViewApiCallSample.record(ApiCall.EVALUATE_JAVASCRIPT);
            checkThread();
            mAwContents.evaluateJavaScript(
                    script, CallbackConverter.fromValueCallback(resultCallback));
        }
    }

    @Override
    public void saveWebArchive(String filename) {
        saveWebArchive(filename, false, null);
    }

    @Override
    public void saveWebArchive(final String basename, final boolean autoname,
            final ValueCallback<String> callback) {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    saveWebArchive(basename, autoname, callback);
                }
            });
            return;
        }
        mAwContents.saveWebArchive(
                basename, autoname, CallbackConverter.fromValueCallback(callback));
    }

    @Override
    public void stopLoading() {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    stopLoading();
                }
            });
            return;
        }

        sWebViewApiCallSample.record(ApiCall.STOP_LOADING);
        mAwContents.stopLoading();
    }

    @Override
    public void reload() {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    reload();
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.RELOAD);
        mAwContents.reload();
    }

    @Override
    public boolean canGoBack() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            Boolean ret = mFactory.runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return canGoBack();
                }
            });
            return ret;
        }
        sWebViewApiCallSample.record(ApiCall.CAN_GO_BACK);
        return mAwContents.canGoBack();
    }

    @Override
    public void goBack() {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    goBack();
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.GO_BACK);
        mAwContents.goBack();
    }

    @Override
    public boolean canGoForward() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            Boolean ret = mFactory.runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return canGoForward();
                }
            });
            return ret;
        }
        sWebViewApiCallSample.record(ApiCall.CAN_GO_FORWARD);
        return mAwContents.canGoForward();
    }

    @Override
    public void goForward() {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    goForward();
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.GO_FORWARD);
        mAwContents.goForward();
    }

    @Override
    public boolean canGoBackOrForward(final int steps) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            Boolean ret = mFactory.runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return canGoBackOrForward(steps);
                }
            });
            return ret;
        }
        sWebViewApiCallSample.record(ApiCall.CAN_GO_BACK_OR_FORWARD);
        return mAwContents.canGoBackOrForward(steps);
    }

    @Override
    public void goBackOrForward(final int steps) {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    goBackOrForward(steps);
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.GO_BACK_OR_FORWARD);
        mAwContents.goBackOrForward(steps);
    }

    @Override
    public boolean isPrivateBrowsingEnabled() {
        // Not supported in this WebView implementation.
        sWebViewApiCallSample.record(ApiCall.IS_PRIVATE_BROWSING_ENABLED);
        return false;
    }

    @Override
    public boolean pageUp(final boolean top) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            Boolean ret = mFactory.runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return pageUp(top);
                }
            });
            return ret;
        }
        sWebViewApiCallSample.record(ApiCall.PAGE_UP);
        return mAwContents.pageUp(top);
    }

    @Override
    public boolean pageDown(final boolean bottom) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            Boolean ret = mFactory.runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return pageDown(bottom);
                }
            });
            return ret;
        }
        sWebViewApiCallSample.record(ApiCall.PAGE_DOWN);
        return mAwContents.pageDown(bottom);
    }

    @Override
    @TargetApi(Build.VERSION_CODES.M)
    public void insertVisualStateCallback(
            final long requestId, final VisualStateCallback callback) {
        sWebViewApiCallSample.record(ApiCall.INSERT_VISUAL_STATE_CALLBACK);
        if (callback == null) return;
        mSharedWebViewChromium.insertVisualStateCallback(
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
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    clearView();
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.CLEAR_VIEW);
        mAwContents.clearView();
    }

    @Override
    public Picture capturePicture() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            Picture ret = mFactory.runOnUiThreadBlocking(new Callable<Picture>() {
                @Override
                public Picture call() {
                    return capturePicture();
                }
            });
            return ret;
        }
        sWebViewApiCallSample.record(ApiCall.CAPTURE_PICTURE);
        return mAwContents.capturePicture();
    }

    @Override
    public float getScale() {
        sWebViewApiCallSample.record(ApiCall.GET_SCALE);
        // No checkThread() as it is mostly thread safe (workaround for b/10652991).
        mFactory.startYourEngines(true);
        return mAwContents.getScale();
    }

    @Override
    public void setInitialScale(final int scaleInPercent) {
        sWebViewApiCallSample.record(ApiCall.SET_INITIAL_SCALE);
        // No checkThread() as it is thread safe
        mWebSettings.getAwSettings().setInitialPageScale(scaleInPercent);
    }

    @Override
    public void invokeZoomPicker() {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    invokeZoomPicker();
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.INVOKE_ZOOM_PICKER);
        mAwContents.invokeZoomPicker();
    }

    @Override
    public WebView.HitTestResult getHitTestResult() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            WebView.HitTestResult ret =
                    mFactory.runOnUiThreadBlocking(new Callable<WebView.HitTestResult>() {
                        @Override
                        public WebView.HitTestResult call() {
                            return getHitTestResult();
                        }
                    });
            return ret;
        }
        sWebViewApiCallSample.record(ApiCall.GET_HIT_TEST_RESULT);
        AwContents.HitTestData data = mAwContents.getLastHitTestResult();
        mHitTestResult.setType(data.hitTestResultType);
        mHitTestResult.setExtra(data.hitTestResultExtraData);
        return mHitTestResult;
    }

    @Override
    public void requestFocusNodeHref(final Message hrefMsg) {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    requestFocusNodeHref(hrefMsg);
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.REQUEST_FOCUS_NODE_HREF);
        mAwContents.requestFocusNodeHref(hrefMsg);
    }

    @Override
    public void requestImageRef(final Message msg) {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    requestImageRef(msg);
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.REQUEST_IMAGE_REF);
        mAwContents.requestImageRef(msg);
    }

    @Override
    public String getUrl() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            String ret = mFactory.runOnUiThreadBlocking(new Callable<String>() {
                @Override
                public String call() {
                    return getUrl();
                }
            });
            return ret;
        }
        sWebViewApiCallSample.record(ApiCall.GET_URL);
        return mAwContents.getUrl();
    }

    @Override
    public String getOriginalUrl() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            String ret = mFactory.runOnUiThreadBlocking(new Callable<String>() {
                @Override
                public String call() {
                    return getOriginalUrl();
                }
            });
            return ret;
        }
        sWebViewApiCallSample.record(ApiCall.GET_ORIGINAL_URL);
        return mAwContents.getOriginalUrl();
    }

    @Override
    public String getTitle() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            String ret = mFactory.runOnUiThreadBlocking(new Callable<String>() {
                @Override
                public String call() {
                    return getTitle();
                }
            });
            return ret;
        }
        sWebViewApiCallSample.record(ApiCall.GET_TITLE);
        return mAwContents.getTitle();
    }

    @Override
    public Bitmap getFavicon() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            Bitmap ret = mFactory.runOnUiThreadBlocking(new Callable<Bitmap>() {
                @Override
                public Bitmap call() {
                    return getFavicon();
                }
            });
            return ret;
        }
        sWebViewApiCallSample.record(ApiCall.GET_FAVICON);
        return mAwContents.getFavicon();
    }

    @Override
    public String getTouchIconUrl() {
        // Intentional no-op: hidden method on WebView.
        return null;
    }

    @Override
    public int getProgress() {
        sWebViewApiCallSample.record(ApiCall.GET_PROGRESS);
        if (mAwContents == null) return 100;
        // No checkThread() because the value is cached java side (workaround for b/10533304).
        return mAwContents.getMostRecentProgress();
    }

    @Override
    public int getContentHeight() {
        sWebViewApiCallSample.record(ApiCall.GET_CONTENT_HEIGHT);
        if (mAwContents == null) return 0;
        // No checkThread() as it is mostly thread safe (workaround for b/10594869).
        return mAwContents.getContentHeightCss();
    }

    @Override
    public int getContentWidth() {
        sWebViewApiCallSample.record(ApiCall.GET_CONTENT_WIDTH);
        if (mAwContents == null) return 0;
        // No checkThread() as it is mostly thread safe (workaround for b/10594869).
        return mAwContents.getContentWidthCss();
    }

    @Override
    public void pauseTimers() {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    pauseTimers();
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.PAUSE_TIMERS);
        mAwContents.pauseTimers();
    }

    @Override
    public void resumeTimers() {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    resumeTimers();
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.RESUME_TIMERS);
        mAwContents.resumeTimers();
    }

    @Override
    public void onPause() {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    onPause();
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.ON_PAUSE);
        mAwContents.onPause();
    }

    @Override
    public void onResume() {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    onResume();
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.ON_RESUME);
        mAwContents.onResume();
    }

    @Override
    public boolean isPaused() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            Boolean ret = mFactory.runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return isPaused();
                }
            });
            return ret;
        }
        sWebViewApiCallSample.record(ApiCall.IS_PAUSED);
        return mAwContents.isPaused();
    }

    @Override
    public void freeMemory() {
        // Intentional no-op. Memory is managed automatically by Chromium.
    }

    @Override
    public void clearCache(final boolean includeDiskFiles) {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    clearCache(includeDiskFiles);
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.CLEAR_CACHE);
        mAwContents.clearCache(includeDiskFiles);
    }

    /**
     * This is a poorly named method, but we keep it for historical reasons.
     */
    @Override
    public void clearFormData() {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    clearFormData();
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.CLEAR_FORM_DATA);
        mAwContents.hideAutofillPopup();
    }

    @Override
    public void clearHistory() {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    clearHistory();
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.CLEAR_HISTORY);
        mAwContents.clearHistory();
    }

    @Override
    public void clearSslPreferences() {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    clearSslPreferences();
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.CLEAR_SSL_PREFERENCES);
        mAwContents.clearSslPreferences();
    }

    @Override
    public WebBackForwardList copyBackForwardList() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            WebBackForwardList ret =
                    mFactory.runOnUiThreadBlocking(new Callable<WebBackForwardList>() {
                        @Override
                        public WebBackForwardList call() {
                            return copyBackForwardList();
                        }
                    });
            return ret;
        }
        sWebViewApiCallSample.record(ApiCall.COPY_BACK_FORWARD_LIST);
        // mAwContents.getNavigationHistory() can be null here if mAwContents has been destroyed,
        // and we do not handle passing null to the WebBackForwardListChromium constructor.
        NavigationHistory navHistory = mAwContents.getNavigationHistory();
        if (navHistory == null) navHistory = new NavigationHistory();
        return new WebBackForwardListChromium(navHistory);
    }

    @Override
    public void setFindListener(WebView.FindListener listener) {
        sWebViewApiCallSample.record(ApiCall.SET_FIND_LISTENER);
        mContentsClientAdapter.setFindListener(listener);
    }

    @Override
    public void findNext(final boolean forwards) {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    findNext(forwards);
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.FIND_NEXT);
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
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    findAllAsync(searchString);
                }
            });
            return;
        }
        mAwContents.findAllAsync(searchString);
    }

    @Override
    public boolean showFindDialog(final String text, final boolean showIme) {
        sWebViewApiCallSample.record(ApiCall.SHOW_FIND_DIALOG);
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
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    notifyFindDialogDismissed();
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.NOTIFY_FIND_DIALOG_DISMISSED);
        clearMatches();
    }

    @Override
    public void clearMatches() {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    clearMatches();
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.CLEAR_MATCHES);
        mAwContents.clearMatches();
    }

    @Override
    public void documentHasImages(final Message response) {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    documentHasImages(response);
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.DOCUMENT_HAS_IMAGES);
        mAwContents.documentHasImages(response);
    }

    @Override
    public void setWebViewClient(WebViewClient client) {
        sWebViewApiCallSample.record(ApiCall.SET_WEBVIEW_CLIENT);
        mSharedWebViewChromium.setWebViewClient(client);
        mContentsClientAdapter.setWebViewClient(mSharedWebViewChromium.getWebViewClient());
    }

    @Override
    public WebViewClient getWebViewClient() {
        sWebViewApiCallSample.record(ApiCall.GET_WEBVIEW_CLIENT);
        return mSharedWebViewChromium.getWebViewClient();
    }

    @Override
    public void setDownloadListener(DownloadListener listener) {
        sWebViewApiCallSample.record(ApiCall.SET_DOWNLOAD_LISTENER);
        mContentsClientAdapter.setDownloadListener(listener);
    }

    @Override
    public void setWebChromeClient(WebChromeClient client) {
        sWebViewApiCallSample.record(ApiCall.SET_WEBCHROME_CLIENT);
        mWebSettings.getAwSettings().setFullscreenSupported(doesSupportFullscreen(client));
        mSharedWebViewChromium.setWebChromeClient(client);
        mContentsClientAdapter.setWebChromeClient(mSharedWebViewChromium.getWebChromeClient());
    }

    @Override
    public WebChromeClient getWebChromeClient() {
        sWebViewApiCallSample.record(ApiCall.GET_WEBCHROME_CLIENT);
        return mSharedWebViewChromium.getWebChromeClient();
    }

    /**
     * Returns true if the supplied {@link WebChromeClient} supports fullscreen.
     *
     * <p>For fullscreen support, implementations of {@link WebChromeClient#onShowCustomView}
     * and {@link WebChromeClient#onHideCustomView()} are required.
     */
    private boolean doesSupportFullscreen(WebChromeClient client) {
        sWebViewApiCallSample.record(ApiCall.DOES_SUPPORT_FULLSCREEN);
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
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    setPictureListener(listener);
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.SET_PICTURE_LISTENER);
        boolean invalidateOnly = mAppTargetSdkVersion >= Build.VERSION_CODES.JELLY_BEAN_MR2;
        mContentsClientAdapter.setPictureListener(listener, invalidateOnly);
        mAwContents.enableOnNewPicture(listener != null, invalidateOnly);
    }

    @Override
    public void addJavascriptInterface(final Object obj, final String interfaceName) {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    addJavascriptInterface(obj, interfaceName);
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.ADD_JAVASCRIPT_INTERFACE);
        mAwContents.addJavascriptInterface(obj, interfaceName);
    }

    @Override
    public void removeJavascriptInterface(final String interfaceName) {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    removeJavascriptInterface(interfaceName);
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.REMOVE_JAVASCRIPT_INTERFACE);
        mAwContents.removeJavascriptInterface(interfaceName);
    }

    @Override
    public WebMessagePort[] createWebMessageChannel() {
        sWebViewApiCallSample.record(ApiCall.CREATE_WEBMESSAGE_CHANNEL);
        return WebMessagePortAdapter.fromMessagePorts(
                mSharedWebViewChromium.createWebMessageChannel());
    }

    @Override
    @TargetApi(Build.VERSION_CODES.M)
    public void postMessageToMainFrame(final WebMessage message, final Uri targetOrigin) {
        sWebViewApiCallSample.record(ApiCall.POST_MESSAGE_TO_MAIN_FRAME);
        mSharedWebViewChromium.postMessageToMainFrame(message.getData(), targetOrigin.toString(),
                WebMessagePortAdapter.toMessagePorts(message.getPorts()));
    }

    @Override
    public WebSettings getSettings() {
        sWebViewApiCallSample.record(ApiCall.GET_SETTINGS);
        return mWebSettings;
    }

    @Override
    public void setMapTrackballToArrowKeys(boolean setMap) {
        // This is a deprecated API: intentional no-op.
    }

    @Override
    public void flingScroll(final int vx, final int vy) {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
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
        sWebViewApiCallSample.record(ApiCall.CAN_ZOOM_IN);
        if (checkNeedsPost()) {
            return false;
        }
        return mAwContents.canZoomIn();
    }

    @Override
    public boolean canZoomOut() {
        sWebViewApiCallSample.record(ApiCall.CAN_ZOOM_OUT);
        if (checkNeedsPost()) {
            return false;
        }
        return mAwContents.canZoomOut();
    }

    @Override
    public boolean zoomIn() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            boolean ret = mFactory.runOnUiThreadBlocking(new Callable<Boolean>() {
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
            boolean ret = mFactory.runOnUiThreadBlocking(new Callable<Boolean>() {
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

    @Override
    public void setRendererPriorityPolicy(
            int rendererRequestedPriority, boolean waivedWhenNotVisible) {
        @RendererPriority
        int awRendererRequestedPriority;
        switch (rendererRequestedPriority) {
            case WebView.RENDERER_PRIORITY_WAIVED:
                awRendererRequestedPriority = RendererPriority.WAIVED;
                break;
            case WebView.RENDERER_PRIORITY_BOUND:
                awRendererRequestedPriority = RendererPriority.LOW;
                break;
            default:
            case WebView.RENDERER_PRIORITY_IMPORTANT:
                awRendererRequestedPriority = RendererPriority.HIGH;
                break;
        }
        mAwContents.setRendererPriorityPolicy(awRendererRequestedPriority, waivedWhenNotVisible);
    }

    @Override
    public int getRendererRequestedPriority() {
        @RendererPriority
        final int awRendererRequestedPriority = mAwContents.getRendererRequestedPriority();
        switch (awRendererRequestedPriority) {
            case RendererPriority.WAIVED:
                return WebView.RENDERER_PRIORITY_WAIVED;
            case RendererPriority.LOW:
                return WebView.RENDERER_PRIORITY_BOUND;
            default:
            case RendererPriority.HIGH:
                return WebView.RENDERER_PRIORITY_IMPORTANT;
        }
    }

    @Override
    public boolean getRendererPriorityWaivedWhenNotVisible() {
        return mAwContents.getRendererPriorityWaivedWhenNotVisible();
    }

    @Override
    public void setTextClassifier(TextClassifier textClassifier) {
        sWebViewApiCallSample.record(ApiCall.SET_TEXT_CLASSIFIER);
        mAwContents.setTextClassifier(textClassifier);
    }

    @Override
    public TextClassifier getTextClassifier() {
        sWebViewApiCallSample.record(ApiCall.GET_TEXT_CLASSIFIER);
        return mAwContents.getTextClassifier();
    }

    @Override
    public void autofill(final SparseArray<AutofillValue> values) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            mFactory.runVoidTaskOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    autofill(values);
                }
            });
        }
        sWebViewApiCallSample.record(ApiCall.AUTOFILL);
        mAwContents.autofill(values);
    }

    @Override
    public void onProvideAutofillVirtualStructure(final ViewStructure structure, final int flags) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            mFactory.runVoidTaskOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    onProvideAutofillVirtualStructure(structure, flags);
                }
            });
            return;
        }
        sWebViewApiCallSample.record(ApiCall.ON_PROVIDE_AUTOFILL_VIRTUAL_STRUCTURE);
        mAwContents.onProvideAutoFillVirtualStructure(structure, flags);
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
            boolean ret = mFactory.runOnUiThreadBlocking(new Callable<Boolean>() {
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
                    mFactory.runOnUiThreadBlocking(new Callable<AccessibilityNodeProvider>() {
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
            mFactory.runVoidTaskOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    onProvideVirtualStructure(structure);
                }
            });
            return;
        }
        mAwContents.onProvideVirtualStructure(structure);
    }

    @Override
    public void onInitializeAccessibilityNodeInfo(final AccessibilityNodeInfo info) {
        // Intentional no-op. Chromium accessibility implementation currently does not need this
        // calls.
    }

    @Override
    public void onInitializeAccessibilityEvent(final AccessibilityEvent event) {
        // Intentional no-op. Chromium accessibility implementation currently does not need this
        // calls.
    }

    @Override
    public boolean performAccessibilityAction(final int action, final Bundle arguments) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret = mFactory.runOnUiThreadBlocking(new Callable<Boolean>() {
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
            mFactory.addTask(new Runnable() {
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
            mFactory.addTask(new Runnable() {
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
            mFactory.addTask(new Runnable() {
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
            mFactory.addTask(new Runnable() {
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
            mFactory.runVoidTaskOnUiThreadBlocking(new Runnable() {
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
            mFactory.runVoidTaskOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    mAwContents.setLayoutParams(layoutParams);
                }
            });
            return;
        }
        mAwContents.setLayoutParams(layoutParams);
    }

    @Override
    public void onActivityResult(final int requestCode, final int resultCode, final Intent data) {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
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
            mFactory.addTask(new Runnable() {
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
    public boolean onDragEvent(final DragEvent event) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret = mFactory.runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return onDragEvent(event);
                }
            });
            return ret;
        }
        return mAwContents.onDragEvent(event);
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
            boolean ret = mFactory.runOnUiThreadBlocking(new Callable<Boolean>() {
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
            boolean ret = mFactory.runOnUiThreadBlocking(new Callable<Boolean>() {
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
            boolean ret = mFactory.runOnUiThreadBlocking(new Callable<Boolean>() {
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
            mFactory.addTask(new Runnable() {
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
            mFactory.addTask(new Runnable() {
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
            mFactory.addTask(new Runnable() {
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
            mFactory.addTask(new Runnable() {
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
            mFactory.addTask(new Runnable() {
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
            mFactory.addTask(new Runnable() {
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
            boolean ret = mFactory.runOnUiThreadBlocking(new Callable<Boolean>() {
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
            boolean ret = mFactory.runOnUiThreadBlocking(new Callable<Boolean>() {
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
            boolean ret = mFactory.runOnUiThreadBlocking(new Callable<Boolean>() {
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
            boolean ret = mFactory.runOnUiThreadBlocking(new Callable<Boolean>() {
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
            boolean ret = mFactory.runOnUiThreadBlocking(new Callable<Boolean>() {
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
            mFactory.runVoidTaskOnUiThreadBlocking(new Runnable() {
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
            boolean ret = mFactory.runOnUiThreadBlocking(new Callable<Boolean>() {
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
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
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
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
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
    @Override
    public Handler getHandler(Handler originalHandler) {
        return originalHandler;
    }

    // Overrides method added to WebViewProvider.ViewDelegate interface
    // (not called in M and below)
    @Override
    public View findFocus(View originalFocusedView) {
        return originalFocusedView;
    }

    // Remove from superclass
    @Override
    public void preDispatchDraw(Canvas canvas) {
        // TODO(leandrogracia): remove this method from WebViewProvider if we think
        // we won't need it again.
    }

    @Override
    public void onStartTemporaryDetach() {
        mAwContents.onStartTemporaryDetach();
    }

    @Override
    public void onFinishTemporaryDetach() {
        mAwContents.onFinishTemporaryDetach();
    }

    @Override
    public boolean onCheckIsTextEditor() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            return mFactory.runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return onCheckIsTextEditor();
                }
            });
        }
        return mAwContents.onCheckIsTextEditor();
    }

    // WebViewProvider.ScrollDelegate implementation ----------------------------------------------

    @Override
    public int computeHorizontalScrollRange() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            int ret = mFactory.runOnUiThreadBlocking(new Callable<Integer>() {
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
            int ret = mFactory.runOnUiThreadBlocking(new Callable<Integer>() {
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
            int ret = mFactory.runOnUiThreadBlocking(new Callable<Integer>() {
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
            int ret = mFactory.runOnUiThreadBlocking(new Callable<Integer>() {
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
            int ret = mFactory.runOnUiThreadBlocking(new Callable<Integer>() {
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
            mFactory.runVoidTaskOnUiThreadBlocking(new Runnable() {
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
        sWebViewApiCallSample.record(ApiCall.CREATE_PRINT_DOCUMENT_ADAPTER);
        checkThread();
        return new AwPrintDocumentAdapter(mAwContents.getPdfExporter(), documentName);
    }
    // AwContents.NativeDrawFunctorFactory implementation ----------------------------------
    private class WebViewNativeDrawFunctorFactory implements AwContents.NativeDrawFunctorFactory {
        @Override
        public AwContents.NativeDrawGLFunctor createGLFunctor(long context) {
            return new DrawGLFunctor(context, mFactory.getWebViewDelegate());
        }

        @Override
        public AwDrawFnImpl.DrawFnAccess getDrawFnAccess() {
            if (BuildInfo.isAtLeastQ()) {
                return mFactory.getWebViewDelegate();
            }
            return null;
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
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                GlueApiHelperForN.super_startActivityForResult(
                        mWebViewPrivate, intent, requestCode);
            } else {
                try {
                    Method startActivityForResultMethod =
                            View.class.getMethod("startActivityForResult", Intent.class, int.class);
                    startActivityForResultMethod.invoke(mWebView, intent, requestCode);
                } catch (Exception e) {
                    throw new RuntimeException("Invalid reflection", e);
                }
            }
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
        sWebViewApiCallSample.record(ApiCall.EXTRACT_SMART_CLIP_DATA);
        checkThread();
        mAwContents.extractSmartClipData(x, y, width, height);
    }

    // Implements SmartClipProvider
    @Override
    public void setSmartClipResultHandler(final Handler resultHandler) {
        sWebViewApiCallSample.record(ApiCall.SET_SMART_CLIP_RESULT_HANDLER);
        checkThread();
        mAwContents.setSmartClipResultHandler(resultHandler);
    }

    SharedWebViewChromium getSharedWebViewChromium() {
        return mSharedWebViewChromium;
    }
}

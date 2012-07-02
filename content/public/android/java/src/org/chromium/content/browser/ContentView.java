// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;
import android.webkit.DownloadListener;
import android.widget.FrameLayout;

import org.chromium.base.JNINamespace;
import org.chromium.base.WeakContext;
import org.chromium.content.app.AppResource;
import org.chromium.content.common.TraceEvent;

@JNINamespace("content")
public class ContentView extends FrameLayout {

    // The following constants match the ones in chrome/common/page_transition_types.h.
    // Add more if you need them.
    public static final int PAGE_TRANSITION_LINK = 0;
    public static final int PAGE_TRANSITION_TYPED = 1;
    public static final int PAGE_TRANSITION_AUTO_BOOKMARK = 2;
    public static final int PAGE_TRANSITION_START_PAGE = 6;

    private static final String TAG = "ContentView";

    // Personality of the ContentView.
    private int mPersonality;
    // Used when ContentView implements a standalone View.
    public static final int PERSONALITY_VIEW = 0;
    // Used for Chrome.
    public static final int PERSONALITY_CHROME = 1;

    /**
     * Automatically decide the number of renderer processes to use based on device memory class.
     * */
    public static final int MAX_RENDERERS_AUTOMATIC = AndroidBrowserProcess.MAX_RENDERERS_AUTOMATIC;
    /**
     * Use single-process mode that runs the renderer on a separate thread in the main application.
     */
    public static final int MAX_RENDERERS_SINGLE_PROCESS =
            AndroidBrowserProcess.MAX_RENDERERS_SINGLE_PROCESS;

    // Used to avoid enabling zooming in / out in WebView zoom controls
    // if resulting zooming will produce little visible difference.
    private static float WEBVIEW_ZOOM_CONTROLS_EPSILON = 0.007f;

    // content_view_client.cc depends on ContentView.java holding a ref to the current client
    // instance since the native side only holds a weak pointer to the client. We chose this
    // solution over the managed object owning the C++ object's memory since it's a lot simpler
    // in terms of clean up.
    private ContentViewClient mContentViewClient;

    private ContentSettings mContentSettings;

    // Native pointer to C++ ContentView object which will be set by nativeInit()
    private int mNativeContentView = 0;

    private ZoomManager mZoomManager;

    // Cached page scale factor from native
    private float mNativePageScaleFactor = 1.0f;
    private float mNativeMinimumScale = 1.0f;
    private float mNativeMaximumScale = 1.0f;

    // TODO(klobag): this is to avoid a bug in GestureDetector. With multi-touch,
    // mAlwaysInTapRegion is not reset. So when the last finger is up, onSingleTapUp()
    // will be mistakenly fired.
    private boolean mIgnoreSingleTap;

    // The legacy webview DownloadListener.
    private DownloadListener mDownloadListener;
    // ContentViewDownloadDelegate adds support for authenticated downloads
    // and POST downloads. Embedders should prefer ContentViewDownloadDelegate
    // over DownloadListener.
    private ContentViewDownloadDelegate mDownloadDelegate;

    /**
     * Enable multi-process ContentView. This should be called by the application before
     * constructing any ContentView instances. If enabled, ContentView will run renderers in
     * separate processes up to the number of processes specified by maxRenderProcesses. If this is
     * not called then the default is to run the renderer in the main application on a separate
     * thread.
     *
     * @param context Context used to obtain the application context.
     * @param maxRendererProcesses Limit on the number of renderers to use. Each tab runs in its own
     * process until the maximum number of processes is reached. The special value of
     * MAX_RENDERERS_SINGLE_PROCESS requests single-process mode where the renderer will run in the
     * application process in a separate thread. If the special value MAX_RENDERERS_AUTOMATIC is
     * used then the number of renderers will be determined based on the device memory class. The
     * maximum number of allowed renderers is capped by MAX_RENDERERS_LIMIT.
     */
    public static void enableMultiProcess(Context context, int maxRendererProcesses) {
        AndroidBrowserProcess.initContentViewProcess(context, maxRendererProcesses);
    }

    public ContentView(Context context, int nativeWebContents, int personality) {
        this(context, nativeWebContents, null, android.R.attr.webViewStyle, personality);
    }

    private ContentView(Context context, int nativeWebContents, AttributeSet attrs, int defStyle,
            int personality) {
        super(context, attrs, defStyle);

        WeakContext.initializeWeakContext(context);
        // By default, ContentView will initialize single process mode. The call to
        // initContentViewProcess below is ignored if either the ContentView host called
        // enableMultiProcess() or the platform browser called initChromiumBrowserProcess().
        AndroidBrowserProcess.initContentViewProcess(context, MAX_RENDERERS_SINGLE_PROCESS);

        initialize(context, nativeWebContents, personality);
    }

    // TODO(jrg): incomplete; upstream the rest of this method.
    private void initialize(Context context, int nativeWebContents, int personality) {
        mNativeContentView = nativeInit(nativeWebContents);

        mPersonality = personality;
        mContentSettings = new ContentSettings(this, mNativeContentView);

        initGestureDetectors(context);

        Log.i(TAG, "mNativeContentView=0x"+ Integer.toHexString(mNativeContentView));
    }

    /**
     * @return Whether the configured personality of this ContentView is {@link #PERSONALITY_VIEW}.
     */
    boolean isPersonalityView() {
        switch (mPersonality) {
            case PERSONALITY_VIEW:
                return true;
            case PERSONALITY_CHROME:
                return false;
            default:
                Log.e(TAG, "Unknown ContentView personality: " + mPersonality);
                return false;
        }
    }


    /**
     * Destroy the internal state of the WebView. This method may only be called
     * after the WebView has been removed from the view system. No other methods
     * may be called on this WebView after this method has been called.
     */
    // TODO(jrg): incomplete; upstream the rest of this method.
    public void destroy() {
        if (mNativeContentView != 0) {
            nativeDestroy(mNativeContentView);
            mNativeContentView = 0;
        }
        if (mContentSettings != null) {
            mContentSettings.destroy();
            mContentSettings = null;
        }
    }

    /**
     * Returns true initially, false after destroy() has been called.
     * It is illegal to call any other public method after destroy().
     */
    public boolean isAlive() {
        return mNativeContentView != 0;
    }

    public void setContentViewClient(ContentViewClient client) {
        if (client == null) {
            throw new IllegalArgumentException("The client can't be null.");
        }
        mContentViewClient = client;
        if (mNativeContentView != 0) {

            // TODO(jrg): upstream this chain.  nativeSetClient(),
            // ContentView::SetClient(), add a content_view_client_,
            // add web_contents_, pass web_contents into native ContentView ctor, ...
            /* nativeSetClient(mNativeContentView, mContentViewClient); */
        }
    }

    ContentViewClient getContentViewClient() {
        if (mContentViewClient == null) {
            // We use the Null Object pattern to avoid having to perform a null check in this class.
            // We create it lazily because most of the time a client will be set almost immediately
            // after ContentView is created.
            mContentViewClient = new ContentViewClient();
            // We don't set the native ContentViewClient pointer here on purpose. The native
            // implementation doesn't mind a null delegate and using one is better than passing a
            // Null Object, since we cut down on the number of JNI calls.
        }
        return mContentViewClient;
    }

    /**
     * Load url without fixing up the url string. Consumers of ContentView are responsible for
     * ensuring the URL passed in is properly formatted (i.e. the scheme has been added if left
     * off during user input).
     *
     * @param url The url to load.
     */
    public void loadUrlWithoutUrlSanitization(String url) {
        loadUrlWithoutUrlSanitization(url, PAGE_TRANSITION_TYPED);
    }

    /**
     * Load url without fixing up the url string. Consumers of ContentView are responsible for
     * ensuring the URL passed in is properly formatted (i.e. the scheme has been added if left
     * off during user input).
     *
     * @param url The url to load.
     * @param pageTransition Page transition id that describes the action that led to this
     *                       navigation. It is important for ranking URLs in the history so the
     *                       omnibox can report suggestions correctly.
     */
    public void loadUrlWithoutUrlSanitization(String url, int pageTransition) {
        if (mNativeContentView != 0) {
            if (isPersonalityView()) {
                nativeLoadUrlWithoutUrlSanitizationWithUserAgentOverride(
                        mNativeContentView,
                        url,
                        pageTransition,
                        mContentSettings.getUserAgentString());
            } else {
                // Chrome stores overridden UA strings in navigation history
                // items, so they stay the same on going back / forward.
                nativeLoadUrlWithoutUrlSanitization(
                        mNativeContentView,
                        url,
                        pageTransition);
            }
        }
    }

    void setAllUserAgentOverridesInHistory() {
        // TODO(tedchoc): Pass user agent override down to native.
    }

    /**
     * Stops loading the current web contents.
     */
    public void stopLoading() {
        if (mNativeContentView != 0) nativeStopLoading(mNativeContentView);
    }

    /**
     * Get the URL of the current page.
     *
     * @return The URL of the current page.
     */
    public String getUrl() {
        if (mNativeContentView != 0) return nativeGetURL(mNativeContentView);
        return null;
    }

    /**
     * Get the title of the current page.
     *
     * @return The title of the current page.
     */
    public String getTitle() {
        if (mNativeContentView != 0) return nativeGetTitle(mNativeContentView);
        return null;
    }

    /**
     * @return The load progress of current web contents (range is 0 - 100).
     */
    public int getProgress() {
        if (mNativeContentView != 0) {
            return (int) (100.0 * nativeGetLoadProgress(mNativeContentView));
        }
        return 100;
    }

    /**
     * @return Whether the current WebContents has a previous navigation entry.
     */
    public boolean canGoBack() {
        return mNativeContentView != 0 && nativeCanGoBack(mNativeContentView);
    }

    /**
     * @return Whether the current WebContents has a navigation entry after the current one.
     */
    public boolean canGoForward() {
        return mNativeContentView != 0 && nativeCanGoForward(mNativeContentView);
    }

    /**
     * @param offset The offset into the navigation history.
     * @return Whether we can move in history by given offset
     */
    public boolean canGoToOffset(int offset) {
        return mNativeContentView != 0 && nativeCanGoToOffset(mNativeContentView, offset);
    }

    /**
     * Navigates to the specified offset from the "current entry". Does nothing if the offset is out
     * of bounds.
     * @param offset The offset into the navigation history.
     */
    public void goToOffset(int offset) {
        if (mNativeContentView != 0) nativeGoToOffset(mNativeContentView, offset);
    }

    /**
     * Goes to the navigation entry before the current one.
     */
    public void goBack() {
        if (mNativeContentView != 0) nativeGoBack(mNativeContentView);
    }

    /**
     * Goes to the navigation entry following the current one.
     */
    public void goForward() {
        if (mNativeContentView != 0) nativeGoForward(mNativeContentView);
    }

    /**
     * Reload the current page.
     */
    public void reload() {
        if (mNativeContentView != 0) nativeReload(mNativeContentView);
    }

    /**
     * Clears the WebView's page history in both the backwards and forwards
     * directions.
     */
    public void clearHistory() {
        if (mNativeContentView != 0) nativeClearHistory(mNativeContentView);
    }

    /**
     * Start pinch zoom. You must call {@link #pinchEnd} to stop.
     */
    void pinchBegin(long timeMs, int x, int y) {
        if (mNativeContentView != 0) {
            // TODO(tedchoc): Pass pinch begin to native.
        }
    }

    /**
     * Stop pinch zoom.
     */
    void pinchEnd(long timeMs) {
        if (mNativeContentView != 0) {
            // TODO(tedchoc): Pass pinch end to native.
        }
    }

    void setIgnoreSingleTap(boolean value) {
        mIgnoreSingleTap = value;
    }

    /**
     * Modify the ContentView magnification level. The effect of calling this
     * method is exactly as after "pinch zoom".
     *
     * @param timeMs The event time in milliseconds.
     * @param delta The ratio of the new magnification level over the current
     *            magnification level.
     * @param anchorX The magnification anchor (X) in the current view
     *            coordinate.
     * @param anchorY The magnification anchor (Y) in the current view
     *            coordinate.
     */
    void pinchBy(long timeMs, int anchorX, int anchorY, float delta) {
        if (mNativeContentView != 0) {
            // TODO(tedchoc): Pass pinch by to native.
        }
    }

    /**
     * Return the ContentSettings object used to control the settings for this
     * WebView.
     *
     * Note that when ContentView is used in the PERSONALITY_CHROME role,
     * ContentSettings can only be used for retrieving settings values. For
     * modifications, ChromeNativePreferences is to be used.
     * @return A ContentSettings object that can be used to control this WebView's
     *         settings.
     */
    public ContentSettings getContentSettings() {
        return mContentSettings;
    }

    private void initGestureDetectors(final Context context) {
        try {
            TraceEvent.begin();
            // TODO(tedchoc): Upstream the rest of the initialization.
            mZoomManager = new ZoomManager(context, this);
            mZoomManager.updateMultiTouchSupport();
        } finally {
            TraceEvent.end();
        }
    }

    void updateMultiTouchZoomSupport() {
        mZoomManager.updateMultiTouchSupport();
    }

    public boolean isMultiTouchZoomSupported() {
        return mZoomManager.isMultiTouchZoomSupported();
    }

    /**
     * Register the listener to be used when content can not be handled by the
     * rendering engine, and should be downloaded instead. This will replace the
     * current listener.
     * @param listener An implementation of DownloadListener.
     */
    // TODO(nileshagrawal): decide if setDownloadDelegate will be public API. If so,
    // this method should be deprecated and the javadoc should make reference to the
    // fact that a ContentViewDownloadDelegate will be used in preference to a
    // DownloadListener.
    public void setDownloadListener(DownloadListener listener) {
        mDownloadListener = listener;
    }

    // Called by DownloadController.
    DownloadListener downloadListener() {
        return mDownloadListener;
    }

    /**
     * Register the delegate to be used when content can not be handled by
     * the rendering engine, and should be downloaded instead. This will replace
     * the current delegate or existing DownloadListner.
     * Embedders should prefer this over the legacy DownloadListener.
     * @param listener An implementation of ContentViewDownloadDelegate.
     */
    public void setDownloadDelegate(ContentViewDownloadDelegate delegate) {
        mDownloadDelegate = delegate;
    }

    // Called by DownloadController.
    ContentViewDownloadDelegate getDownloadDelegate() {
        return mDownloadDelegate;
    }

    /**
     * @return Whether the native ContentView has crashed.
     */
    public boolean isCrashed() {
        if (mNativeContentView == 0) return false;
        return nativeCrashed(mNativeContentView);
    }

    /**
     * @return Whether a reload happens when this ContentView is activated.
     */
    public boolean needsReload() {
        return mNativeContentView != 0 && nativeNeedsReload(mNativeContentView);
    }

    /**
     * Checks whether the WebView can be zoomed in.
     *
     * @return True if the WebView can be zoomed in.
     */
    // This method uses the term 'zoom' for legacy reasons, but relates
    // to what chrome calls the 'page scale factor'.
    public boolean canZoomIn() {
        return mNativeMaximumScale - mNativePageScaleFactor > WEBVIEW_ZOOM_CONTROLS_EPSILON;
    }

    /**
     * Checks whether the WebView can be zoomed out.
     *
     * @return True if the WebView can be zoomed out.
     */
    // This method uses the term 'zoom' for legacy reasons, but relates
    // to what chrome calls the 'page scale factor'.
    public boolean canZoomOut() {
        return mNativePageScaleFactor - mNativeMinimumScale > WEBVIEW_ZOOM_CONTROLS_EPSILON;
    }

    /**
     * Zooms in the WebView by 25% (or less if that would result in zooming in
     * more than possible).
     *
     * @return True if there was a zoom change, false otherwise.
     */
    // This method uses the term 'zoom' for legacy reasons, but relates
    // to what chrome calls the 'page scale factor'.
    public boolean zoomIn() {
        if (!canZoomIn()) {
            return false;
        }

        if (mNativeContentView == 0) {
            return false;
        }

        long timeMs = System.currentTimeMillis();
        int x = getWidth() / 2;
        int y = getHeight() / 2;
        float delta = 1.25f;

        pinchBegin(timeMs, x, y);
        pinchBy(timeMs, x, y, delta);
        pinchEnd(timeMs);

        return true;
    }

    /**
     * Zooms out the WebView by 20% (or less if that would result in zooming out
     * more than possible).
     *
     * @return True if there was a zoom change, false otherwise.
     */
    // This method uses the term 'zoom' for legacy reasons, but relates
    // to what chrome calls the 'page scale factor'.
    public boolean zoomOut() {
        if (!canZoomOut()) {
            return false;
        }

        if (mNativeContentView == 0) {
            return false;
        }

        long timeMs = System.currentTimeMillis();
        int x = getWidth() / 2;
        int y = getHeight() / 2;
        float delta = 0.8f;

        pinchBegin(timeMs, x, y);
        pinchBy(timeMs, x, y, delta);
        pinchEnd(timeMs);

        return true;
    }

    // Invokes the graphical zoom picker widget for this ContentView.
    public void invokeZoomPicker() {
        if (mContentSettings.supportZoom()) {
            mZoomManager.invokeZoomPicker();
        }
    }

    // Unlike legacy WebView getZoomControls which returns external zoom controls,
    // this method returns built-in zoom controls. This method is used in tests.
    public View getZoomControlsForTest() {
        return mZoomManager.getZoomControlsViewForTest();
    }

    /**
     * Initialize the ContentView native side.
     * Should be called with a valid native WebContents.
     * If nativeInitProcess is never called, the first time this method is called, nativeInitProcess
     * will be called implicitly with the default settings.
     * @param webContentsPtr the ContentView does not create a new native WebContents and uses
     *                       the provided one.
     * @return a native pointer to the native ContentView object.
     */
    private native int nativeInit(int webContentsPtr);

    private native void nativeDestroy(int nativeContentViewImpl);

    private native void nativeLoadUrlWithoutUrlSanitization(int nativeContentViewImpl,
            String url, int pageTransition);
    private native void nativeLoadUrlWithoutUrlSanitizationWithUserAgentOverride(
            int nativeContentViewImpl, String url, int pageTransition, String userAgentOverride);

    private native String nativeGetURL(int nativeContentViewImpl);

    private native String nativeGetTitle(int nativeContentViewImpl);

    private native double nativeGetLoadProgress(int nativeContentViewImpl);

    private native boolean nativeIsIncognito(int nativeContentViewImpl);

    // Returns true if the native side crashed so that java side can draw a sad tab.
    private native boolean nativeCrashed(int nativeContentViewImpl);

    private native boolean nativeCanGoBack(int nativeContentViewImpl);

    private native boolean nativeCanGoForward(int nativeContentViewImpl);

    private native boolean nativeCanGoToOffset(int nativeContentViewImpl, int offset);

    private native void nativeGoToOffset(int nativeContentViewImpl, int offset);

    private native void nativeGoBack(int nativeContentViewImpl);

    private native void nativeGoForward(int nativeContentViewImpl);

    private native void nativeStopLoading(int nativeContentViewImpl);

    private native void nativeReload(int nativeContentViewImpl);

    private native boolean nativeNeedsReload(int nativeContentViewImpl);

    private native void nativeClearHistory(int nativeContentViewImpl);
}

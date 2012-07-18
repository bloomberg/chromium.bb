// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Canvas;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.DownloadListener;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.WeakContext;
import org.chromium.content.common.TraceEvent;

/**
 * Contains all the major functionality necessary to manage the lifecycle of a ContentView without
 * being tied to the view system.
 */
@JNINamespace("content")
public class ContentViewCore {
    private static final String TAG = ContentViewCore.class.getName();

    // The following constants match the ones in chrome/common/page_transition_types.h.
    // Add more if you need them.
    public static final int PAGE_TRANSITION_LINK = 0;
    public static final int PAGE_TRANSITION_TYPED = 1;
    public static final int PAGE_TRANSITION_AUTO_BOOKMARK = 2;
    public static final int PAGE_TRANSITION_START_PAGE = 6;

    /** Translate the find selection into a normal selection. */
    public static final int FIND_SELECTION_ACTION_KEEP_SELECTION = 0;
    /** Clear the find selection. */
    public static final int FIND_SELECTION_ACTION_CLEAR_SELECTION = 1;
    /** Focus and click the selected node (for links). */
    public static final int FIND_SELECTION_ACTION_ACTIVATE_SELECTION = 2;

    // Personality of the ContentView.
    private int mPersonality;
    // Used when ContentView implements a standalone View.
    public static final int PERSONALITY_VIEW = 0;
    // Used for Chrome.
    public static final int PERSONALITY_CHROME = 1;

    // Used to avoid enabling zooming in / out in WebView zoom controls
    // if resulting zooming will produce little visible difference.
    private static float WEBVIEW_ZOOM_CONTROLS_EPSILON = 0.007f;

    /**
     * Interface that consumers of {@link ContentViewCore} must implement to allow the proper
     * dispatching of view methods through the containing view.
     *
     * <p>
     * All methods with the "super_" prefix should be routed to the parent of the
     * implementing container view.
     */
    @SuppressWarnings("javadoc")
    public static interface InternalAccessDelegate {
        /**
         * @see View#drawChild(Canvas, View, long)
         */
        boolean drawChild(Canvas canvas, View child, long drawingTime);

        /**
         * @see View#onKeyUp(keyCode, KeyEvent)
         */
        boolean super_onKeyUp(int keyCode, KeyEvent event);

        /**
         * @see View#dispatchKeyEventPreIme(KeyEvent)
         */
        boolean super_dispatchKeyEventPreIme(KeyEvent event);

        /**
         * @see View#dispatchKeyEvent(KeyEvent)
         */
        boolean super_dispatchKeyEvent(KeyEvent event);

        /**
         * @see View#onGenericMotionEvent(MotionEvent)
         */
        boolean super_onGenericMotionEvent(MotionEvent event);

        /**
         * @see View#onConfigurationChanged(Configuration)
         */
        void super_onConfigurationChanged(Configuration newConfig);

        /**
         * @see View#onScrollChanged(int, int, int, int)
         */
        void onScrollChanged(int l, int t, int oldl, int oldt);

        /**
         * @see View#awakenScrollBars()
         */
        boolean awakenScrollBars();

        /**
         * @see View#awakenScrollBars(int, boolean)
         */
        boolean super_awakenScrollBars(int startDelay, boolean invalidate);
    }

    private Context mContext;
    private ViewGroup mContainerView;
    private InternalAccessDelegate mContainerViewInternals;

    // content_view_client.cc depends on ContentViewCore.java holding a ref to the current client
    // instance since the native side only holds a weak pointer to the client. We chose this
    // solution over the managed object owning the C++ object's memory since it's a lot simpler
    // in terms of clean up.
    private ContentViewClient mContentViewClient;

    private ContentSettings mContentSettings;

    // Native pointer to C++ ContentView object which will be set by nativeInit()
    private int mNativeContentViewCore = 0;

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

    /**
     * Initialize the process as the platform browser. This must be called before accessing
     * ContentView in order to treat this as a Chromium browser process.
     *
     * @param context Context used to obtain the application context.
     * @param maxRendererProcesses Same as ContentView.enableMultiProcess()
     * @hide Only used by the platform browser.
     */
    public static void initChromiumBrowserProcess(Context context, int maxRendererProcesses) {
        AndroidBrowserProcess.initChromiumBrowserProcess(context, maxRendererProcesses);
    }

    /**
     * Constructs a new ContentViewCore.
     *
     * @param context The context used to create this.
     * @param containerView The view that will act as a container for all views created by this.
     * @param internalDispatcher Handles dispatching all hidden or super methods to the
     *                           containerView.
     * @param nativeWebContents A pointer to the native web contents.
     * @param personality The type of ContentViewCore being created.
     */
    public ContentViewCore(
            Context context, ViewGroup containerView,
            InternalAccessDelegate internalDispatcher,
            int nativeWebContents, int personality) {
        mContext = context;
        mContainerView = containerView;
        mContainerViewInternals = internalDispatcher;

        WeakContext.initializeWeakContext(context);
        // By default, ContentView will initialize single process mode. The call to
        // initContentViewProcess below is ignored if either the ContentView host called
        // enableMultiProcess() or the platform browser called initChromiumBrowserProcess().
        AndroidBrowserProcess.initContentViewProcess(
                context, AndroidBrowserProcess.MAX_RENDERERS_SINGLE_PROCESS);

        initialize(context, nativeWebContents, personality);
    }

    /**
     * @return The context used for creating this ContentViewCore.
     */
    public Context getContext() {
        return mContext;
    }

    /**
     * @return The ViewGroup that all view actions of this ContentViewCore should interact with.
     */
    protected ViewGroup getContainerView() {
        return mContainerView;
    }

    // TODO(jrg): incomplete; upstream the rest of this method.
    private void initialize(Context context, int nativeWebContents, int personality) {
        mNativeContentViewCore = nativeInit(nativeWebContents);

        mPersonality = personality;
        mContentSettings = new ContentSettings(this, mNativeContentViewCore);
        mContainerView.setWillNotDraw(false);
        mContainerView.setFocusable(true);
        mContainerView.setFocusableInTouchMode(true);
        if (mContainerView.getScrollBarStyle() == View.SCROLLBARS_INSIDE_OVERLAY) {
            mContainerView.setHorizontalScrollBarEnabled(false);
            mContainerView.setVerticalScrollBarEnabled(false);
        }
        mContainerView.setClickable(true);
        initGestureDetectors(context);

        Log.i(TAG, "mNativeContentView=0x"+ Integer.toHexString(mNativeContentViewCore));
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
    public void destroy() {
        hidePopupDialog();
        if (mNativeContentViewCore != 0) {
            nativeDestroy(mNativeContentViewCore);
            mNativeContentViewCore = 0;
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
        return mNativeContentViewCore != 0;
    }

    /**
     * For internal use. Throws IllegalStateException if mNativeContentView is 0.
     * Use this to ensure we get a useful Java stack trace, rather than a native
     * crash dump, from use-after-destroy bugs in Java code.
     */
    void checkIsAlive() throws IllegalStateException {
        if (!isAlive()) {
            throw new IllegalStateException("ContentView used after destroy() was called");
        }
    }

    public void setContentViewClient(ContentViewClient client) {
        if (client == null) {
            throw new IllegalArgumentException("The client can't be null.");
        }
        mContentViewClient = client;
        if (mNativeContentViewCore != 0) {
            nativeSetClient(mNativeContentViewCore, mContentViewClient);
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
        if (mNativeContentViewCore != 0) {
            if (isPersonalityView()) {
                nativeLoadUrlWithoutUrlSanitizationWithUserAgentOverride(
                        mNativeContentViewCore,
                        url,
                        pageTransition,
                        mContentSettings.getUserAgentString());
            } else {
                // Chrome stores overridden UA strings in navigation history
                // items, so they stay the same on going back / forward.
                nativeLoadUrlWithoutUrlSanitization(
                        mNativeContentViewCore,
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
        if (mNativeContentViewCore != 0) nativeStopLoading(mNativeContentViewCore);
    }

    /**
     * Get the URL of the current page.
     *
     * @return The URL of the current page.
     */
    public String getUrl() {
        if (mNativeContentViewCore != 0) return nativeGetURL(mNativeContentViewCore);
        return null;
    }

    /**
     * Get the title of the current page.
     *
     * @return The title of the current page.
     */
    public String getTitle() {
        if (mNativeContentViewCore != 0) return nativeGetTitle(mNativeContentViewCore);
        return null;
    }

    /**
     * @return The load progress of current web contents (range is 0 - 100).
     */
    public int getProgress() {
        if (mNativeContentViewCore != 0) {
            return (int) (100.0 * nativeGetLoadProgress(mNativeContentViewCore));
        }
        return 100;
    }

    public int getWidth() {
        return mContainerView.getWidth();
    }

    public int getHeight() {
        return mContainerView.getHeight();
    }

    /**
     * @return Whether the current WebContents has a previous navigation entry.
     */
    public boolean canGoBack() {
        return mNativeContentViewCore != 0 && nativeCanGoBack(mNativeContentViewCore);
    }

    /**
     * @return Whether the current WebContents has a navigation entry after the current one.
     */
    public boolean canGoForward() {
        return mNativeContentViewCore != 0 && nativeCanGoForward(mNativeContentViewCore);
    }

    /**
     * @param offset The offset into the navigation history.
     * @return Whether we can move in history by given offset
     */
    public boolean canGoToOffset(int offset) {
        return mNativeContentViewCore != 0 && nativeCanGoToOffset(mNativeContentViewCore, offset);
    }

    /**
     * Navigates to the specified offset from the "current entry". Does nothing if the offset is out
     * of bounds.
     * @param offset The offset into the navigation history.
     */
    public void goToOffset(int offset) {
        if (mNativeContentViewCore != 0) nativeGoToOffset(mNativeContentViewCore, offset);
    }

    /**
     * Goes to the navigation entry before the current one.
     */
    public void goBack() {
        if (mNativeContentViewCore != 0) nativeGoBack(mNativeContentViewCore);
    }

    /**
     * Goes to the navigation entry following the current one.
     */
    public void goForward() {
        if (mNativeContentViewCore != 0) nativeGoForward(mNativeContentViewCore);
    }

    /**
     * Reload the current page.
     */
    public void reload() {
        if (mNativeContentViewCore != 0) nativeReload(mNativeContentViewCore);
    }

    /**
     * Clears the WebView's page history in both the backwards and forwards
     * directions.
     */
    public void clearHistory() {
        if (mNativeContentViewCore != 0) nativeClearHistory(mNativeContentViewCore);
    }

    /**
     * Start pinch zoom. You must call {@link #pinchEnd} to stop.
     */
    void pinchBegin(long timeMs, int x, int y) {
        if (mNativeContentViewCore != 0) {
            // TODO(tedchoc): Pass pinch begin to native.
        }
    }

    /**
     * Stop pinch zoom.
     */
    void pinchEnd(long timeMs) {
        if (mNativeContentViewCore != 0) {
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
        if (mNativeContentViewCore != 0) {
            // TODO(tedchoc): Pass pinch by to native.
        }
    }

    /**
     * This method should be called when the containing activity is paused
     *
     * @hide
     **/
    public void onActivityPause() {
        TraceEvent.begin();
        hidePopupDialog();
        TraceEvent.end();
    }

    /**
     * Called when the WebView is hidden.
     *
     * @hide
     **/
    public void onHide() {
        hidePopupDialog();
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

    private void hidePopupDialog() {
        SelectPopupDialog.hide(this);
    }

    // End FrameLayout overrides.

    /**
     * @see View#awakenScrollBars(int, boolean)
     */
    @SuppressWarnings("javadoc")
    protected boolean awakenScrollBars(int startDelay, boolean invalidate) {
        // For the default implementation of ContentView which draws the scrollBars on the native
        // side, calling this function may get us into a bad state where we keep drawing the
        // scrollBars, so disable it by always returning false.
        if (mContainerView.getScrollBarStyle() == View.SCROLLBARS_INSIDE_OVERLAY) {
            return false;
        } else {
            return mContainerViewInternals.super_awakenScrollBars(startDelay, invalidate);
        }
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

    void selectPopupMenuItems(int[] indices) {
        if (mNativeContentViewCore != 0) {
            nativeSelectPopupMenuItems(mNativeContentViewCore, indices);
        }
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
        if (mNativeContentViewCore == 0) return false;
        return nativeCrashed(mNativeContentViewCore);
    }

    // The following methods are called by native through jni

    /**
     * Called (from native) when the <select> popup needs to be shown.
     * @param items           Items to show.
     * @param enabled         POPUP_ITEM_TYPEs for items.
     * @param multiple        Whether the popup menu should support multi-select.
     * @param selectedIndices Indices of selected items.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void showSelectPopup(String[] items, int[] enabled, boolean multiple,
            int[] selectedIndices) {
        SelectPopupDialog.show(this, items, enabled, multiple, selectedIndices);
    }

    /**
     * Called (from native) when page loading begins.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void didStartLoading() {
        hidePopupDialog();
    }

    /**
     * @return Whether a reload happens when this ContentView is activated.
     */
    public boolean needsReload() {
        return mNativeContentViewCore != 0 && nativeNeedsReload(mNativeContentViewCore);
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

        if (mNativeContentViewCore == 0) {
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

        if (mNativeContentViewCore == 0) {
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

    @CalledByNative
    private void startContentIntent(String contentUrl) {
        getContentViewClient().onStartContentIntent(getContext(), contentUrl);
    }

    // The following methods are implemented at native side.

    /**
     * Initialize the ContentView native side.
     * Should be called with a valid native WebContents.
     * If nativeInitProcess is never called, the first time this method is called,
     * nativeInitProcess will be called implicitly with the default settings.
     * @param webContentsPtr the ContentView does not create a new native WebContents and uses
     *                       the provided one.
     * @return a native pointer to the native ContentView object.
     */
    private native int nativeInit(int webContentsPtr);

    private native void nativeDestroy(int nativeContentViewCoreImpl);

    private native void nativeLoadUrlWithoutUrlSanitization(int nativeContentViewCoreImpl,
            String url, int pageTransition);
    private native void nativeLoadUrlWithoutUrlSanitizationWithUserAgentOverride(
            int nativeContentViewCoreImpl, String url, int pageTransition,
            String userAgentOverride);

    private native String nativeGetURL(int nativeContentViewCoreImpl);

    private native String nativeGetTitle(int nativeContentViewCoreImpl);

    private native double nativeGetLoadProgress(int nativeContentViewCoreImpl);

    private native boolean nativeIsIncognito(int nativeContentViewCoreImpl);

    // Returns true if the native side crashed so that java side can draw a sad tab.
    private native boolean nativeCrashed(int nativeContentViewCoreImpl);

    private native boolean nativeCanGoBack(int nativeContentViewCoreImpl);

    private native boolean nativeCanGoForward(int nativeContentViewCoreImpl);

    private native boolean nativeCanGoToOffset(int nativeContentViewCoreImpl, int offset);

    private native void nativeGoToOffset(int nativeContentViewCoreImpl, int offset);

    private native void nativeGoBack(int nativeContentViewCoreImpl);

    private native void nativeGoForward(int nativeContentViewCoreImpl);

    private native void nativeStopLoading(int nativeContentViewCoreImpl);

    private native void nativeReload(int nativeContentViewCoreImpl);

    private native void nativeSelectPopupMenuItems(int nativeContentViewCoreImpl, int[] indices);

    private native void nativeSetClient(int nativeContentViewCoreImpl, ContentViewClient client);

    private native boolean nativeNeedsReload(int nativeContentViewCoreImpl);

    private native void nativeClearHistory(int nativeContentViewCoreImpl);
}

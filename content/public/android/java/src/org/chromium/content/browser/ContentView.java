// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.os.Build;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.widget.FrameLayout;

import org.chromium.content.common.TraceEvent;
import org.chromium.ui.gfx.NativeWindow;

import java.util.ArrayList;

/**
 * The containing view for {@link ContentViewCore} that exists in the Android UI hierarchy and
 * exposes the various {@link View} functionality to it.
 *
 * TODO(joth): Remove any methods overrides from this class that were added for WebView
 *             compatibility.
 */
public class ContentView extends FrameLayout implements ContentViewCore.InternalAccessDelegate {
    // Used when ContentView implements a standalone View.
    public static final int PERSONALITY_VIEW = ContentViewCore.PERSONALITY_VIEW;
    // Used for Chrome.
    public static final int PERSONALITY_CHROME = ContentViewCore.PERSONALITY_CHROME;

    /**
     * Automatically decide the number of renderer processes to use based on device memory class.
     * */
    public static final int MAX_RENDERERS_AUTOMATIC = AndroidBrowserProcess.MAX_RENDERERS_AUTOMATIC;
    /**
     * Use single-process mode that runs the renderer on a separate thread in the main application.
     */
    public static final int MAX_RENDERERS_SINGLE_PROCESS =
            AndroidBrowserProcess.MAX_RENDERERS_SINGLE_PROCESS;
    /**
     * Cap on the maximum number of renderer processes that can be requested.
     */
    public static final int MAX_RENDERERS_LIMIT = AndroidBrowserProcess.MAX_RENDERERS_LIMIT;

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
     * @return Whether the process actually needed to be initialized (false if already running).
     */
    public static boolean enableMultiProcess(Context context, int maxRendererProcesses) {
        return ContentViewCore.enableMultiProcess(context, maxRendererProcesses);
    }

    /**
     * Initialize the process as the platform browser. This must be called before accessing
     * ContentView in order to treat this as a Chromium browser process.
     *
     * @param context Context used to obtain the application context.
     * @param maxRendererProcesses Same as ContentView.enableMultiProcess()
     * @return Whether the process actually needed to be initialized (false if already running).
     */
    public static boolean initChromiumBrowserProcess(Context context, int maxRendererProcesses) {
        return ContentViewCore.initChromiumBrowserProcess(context, maxRendererProcesses);
    }

    private ContentViewCore mContentViewCore;

    /**
     * Creates an instance of a ContentView.
     * @param context The Context the view is running in, through which it can
     *                access the current theme, resources, etc.
     * @param nativeWebContents A pointer to the native web contents.
     * @param nativeWindow An instance of the NativeWindow.
     * @param personality One of {@link #PERSONALITY_CHROME} or {@link #PERSONALITY_VIEW}.
     * @return A ContentView instance.
     */
    public static ContentView newInstance(Context context, int nativeWebContents,
            NativeWindow nativeWindow, int personality) {
        return newInstance(context, nativeWebContents, nativeWindow, null,
                android.R.attr.webViewStyle, personality);
    }

    /**
     * Creates an instance of a ContentView.
     * @param context The Context the view is running in, through which it can
     *                access the current theme, resources, etc.
     * @param nativeWebContents A pointer to the native web contents.
     * @param nativeWindow An instance of the NativeWindow.
     * @param attrs The attributes of the XML tag that is inflating the view.
     * @return A ContentView instance.
     */
    public static ContentView newInstance(Context context, int nativeWebContents,
            NativeWindow nativeWindow, AttributeSet attrs) {
        // TODO(klobag): use the WebViewStyle as the default style for now. It enables scrollbar.
        // When ContentView is moved to framework, we can define its own style in the res.
        return newInstance(context, nativeWebContents, nativeWindow, attrs,
                android.R.attr.webViewStyle);
    }

    /**
     * Creates an instance of a ContentView.
     * @param context The Context the view is running in, through which it can
     *                access the current theme, resources, etc.
     * @param nativeWebContents A pointer to the native web contents.
     * @param nativeWindow An instance of the NativeWindow.
     * @param attrs The attributes of the XML tag that is inflating the view.
     * @param defStyle The default style to apply to this view.
     * @return A ContentView instance.
     */
    public static ContentView newInstance(Context context, int nativeWebContents,
            NativeWindow nativeWindow, AttributeSet attrs, int defStyle) {
        return newInstance(context, nativeWebContents, nativeWindow, attrs, defStyle,
                PERSONALITY_VIEW);
    }

    private static ContentView newInstance(Context context, int nativeWebContents,
            NativeWindow nativeWindow, AttributeSet attrs, int defStyle, int personality) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN) {
            return new ContentView(context, nativeWebContents, nativeWindow, attrs, defStyle,
                    personality);
        } else {
            return new JellyBeanContentView(context, nativeWebContents, nativeWindow, attrs,
                    defStyle, personality);
        }
    }

    protected ContentView(Context context, int nativeWebContents, NativeWindow nativeWindow,
            AttributeSet attrs, int defStyle, int personality) {
        super(context, attrs, defStyle);

        mContentViewCore = new ContentViewCore(context, personality);
        mContentViewCore.initialize(this, this, nativeWebContents, nativeWindow, false);
    }

    /**
     * @return The core component of the ContentView that handles JNI communication.  Should only be
     *         used for passing to native.
     */
    public ContentViewCore getContentViewCore() {
        return mContentViewCore;
    }

    /**
     * Returns true if the given Activity has hardware acceleration enabled
     * in its manifest, or in its foreground window.
     *
     * TODO(husky): Remove when ContentViewCore.initialize() is refactored (see TODO there)
     * TODO(dtrainor) This is still used by other classes.  Make sure to pull some version of this
     * out before removing it.
     */
    public static boolean hasHardwareAcceleration(Activity activity) {
        return ContentViewCore.hasHardwareAcceleration(activity);
    }

    /**
     * @return Whether the configured personality of this ContentView is {@link #PERSONALITY_VIEW}.
     */
    boolean isPersonalityView() {
        return mContentViewCore.isPersonalityView();
    }

    /**
     * Destroy the internal state of the WebView. This method may only be called
     * after the WebView has been removed from the view system. No other methods
     * may be called on this WebView after this method has been called.
     */
    public void destroy() {
        mContentViewCore.destroy();
    }

    /**
     * Returns true initially, false after destroy() has been called.
     * It is illegal to call any other public method after destroy().
     */
    public boolean isAlive() {
        return mContentViewCore.isAlive();
    }

    /**
     * For internal use. Throws IllegalStateException if mNativeContentView is 0.
     * Use this to ensure we get a useful Java stack trace, rather than a native
     * crash dump, from use-after-destroy bugs in Java code.
     */
    void checkIsAlive() throws IllegalStateException {
        mContentViewCore.checkIsAlive();
    }

    public void setContentViewClient(ContentViewClient client) {
        mContentViewCore.setContentViewClient(client);
    }

    // @VisibleForTesting
    public ContentViewClient getContentViewClient() {
        return mContentViewCore.getContentViewClient();
    }

    public int getBackgroundColor() {
        return mContentViewCore.getBackgroundColor();
    }

    /**
     * Load url without fixing up the url string. Consumers of ContentView are responsible for
     * ensuring the URL passed in is properly formatted (i.e. the scheme has been added if left
     * off during user input).
     *
     * @param pararms Parameters for this load.
     */
    public void loadUrl(LoadUrlParams params) {
        mContentViewCore.loadUrl(params);
    }

    void setAllUserAgentOverridesInHistory() {
        mContentViewCore.setAllUserAgentOverridesInHistory();
    }

    /**
     * Stops loading the current web contents.
     */
    public void stopLoading() {
        mContentViewCore.stopLoading();
    }

    /**
     * Get the URL of the current page.
     *
     * @return The URL of the current page.
     */
    public String getUrl() {
        return mContentViewCore.getUrl();
    }

    /**
     * Get the title of the current page.
     *
     * @return The title of the current page.
     */
    public String getTitle() {
        return mContentViewCore.getTitle();
    }

    public Bitmap getBitmap() {
        return getBitmap(getWidth(), getHeight());
    }

    public Bitmap getBitmap(int width, int height) {
        return mContentViewCore.getBitmap(width, height);
    }

    /**
     * @return Whether the ContentView is covered by an overlay that is more than half
     *         of it's surface. This is used to determine if we need to do a slow bitmap capture or
     *         to show the ContentView without them.
     */
    public boolean hasLargeOverlay() {
        return mContentViewCore.hasLargeOverlay();
    }

    /**
     * @return Whether the current WebContents has a previous navigation entry.
     */
    public boolean canGoBack() {
        return mContentViewCore.canGoBack();
    }

    /**
     * @return Whether the current WebContents has a navigation entry after the current one.
     */
    public boolean canGoForward() {
        return mContentViewCore.canGoForward();
    }

    /**
     * @param offset The offset into the navigation history.
     * @return Whether we can move in history by given offset
     */
    public boolean canGoToOffset(int offset) {
        return mContentViewCore.canGoToOffset(offset);
    }

    /**
     * Navigates to the specified offset from the "current entry". Does nothing if the offset is out
     * of bounds.
     * @param offset The offset into the navigation history.
     */
    public void goToOffset(int offset) {
        mContentViewCore.goToOffset(offset);
    }

    /**
     * Goes to the navigation entry before the current one.
     */
    public void goBack() {
        mContentViewCore.goBack();
    }

    /**
     * Goes to the navigation entry following the current one.
     */
    public void goForward() {
        mContentViewCore.goForward();
    }

    /**
     * Reload the current page.
     */
    public void reload() {
        mContentViewCore.reload();
    }

    /**
     * Clears the WebView's page history in both the backwards and forwards
     * directions.
     */
    public void clearHistory() {
        mContentViewCore.clearHistory();
    }

    String getSelectedText() {
        return mContentViewCore.getSelectedText();
    }

    /**
     * Start profiling the update speed. You must call {@link #stopFpsProfiling}
     * to stop profiling.
     *
     * @VisibleForTesting
     */
    public void startFpsProfiling() {
        // TODO(nileshagrawal): Implement this.
    }

    /**
     * Stop profiling the update speed.
     *
     * @VisibleForTesting
     */
    public float stopFpsProfiling() {
        // TODO(nileshagrawal): Implement this.
        return 0.0f;
    }

    /**
     * Fling the ContentView from the current position.
     * @param x Fling touch starting position
     * @param y Fling touch starting position
     * @param velocityX Initial velocity of the fling (X) measured in pixels per second.
     * @param velocityY Initial velocity of the fling (Y) measured in pixels per second.
     *
     * @VisibleForTesting
     */
    public void fling(long timeMs, int x, int y, int velocityX, int velocityY) {
        mContentViewCore.getContentViewGestureHandler().fling(timeMs, x, y, velocityX, velocityY);
    }

    void endFling(long timeMs) {
        mContentViewCore.getContentViewGestureHandler().endFling(timeMs);
    }

    /**
     * Start pinch zoom. You must call {@link #pinchEnd} to stop.
     *
     * @VisibleForTesting
     */
    public void pinchBegin(long timeMs, int x, int y) {
        mContentViewCore.getContentViewGestureHandler().pinchBegin(timeMs, x, y);
    }

    /**
     * Stop pinch zoom.
     *
     * @VisibleForTesting
     */
    public void pinchEnd(long timeMs) {
        mContentViewCore.getContentViewGestureHandler().pinchEnd(timeMs);
    }

    void setIgnoreSingleTap(boolean value) {
        mContentViewCore.getContentViewGestureHandler().setIgnoreSingleTap(value);
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
     *
     * @VisibleForTesting
     */
    public void pinchBy(long timeMs, int anchorX, int anchorY, float delta) {
        mContentViewCore.getContentViewGestureHandler().pinchBy(timeMs, anchorX, anchorY, delta);
    }

    /**
     * Injects the passed JavaScript code in the current page and evaluates it.
     *
     * @throws IllegalStateException If the ContentView has been destroyed.
     */
    public void evaluateJavaScript(String script) throws IllegalStateException {
        mContentViewCore.evaluateJavaScript(script, null);
    }

    /**
     * This method should be called when the containing activity is paused.
     **/
    public void onActivityPause() {
        mContentViewCore.onActivityPause();
    }

    /**
     * This method should be called when the containing activity is resumed.
     **/
    public void onActivityResume() {
        mContentViewCore.onActivityResume();
    }

    /**
     * To be called when the ContentView is shown.
     **/
    public void onShow() {
        mContentViewCore.onShow();
    }

    /**
     * To be called when the ContentView is hidden.
     **/
    public void onHide() {
        mContentViewCore.onHide();
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
        return mContentViewCore.getContentSettings();
    }

    /**
     * Hides the select action bar.
     */
    public void hideSelectActionBar() {
        mContentViewCore.hideSelectActionBar();
    }

    // FrameLayout overrides.

    // Needed by ContentViewCore.InternalAccessDelegate
    @Override
    public boolean drawChild(Canvas canvas, View child, long drawingTime) {
        return super.drawChild(canvas, child, drawingTime);
    }

    // Needed by ContentViewCore.InternalAccessDelegate
    @Override
    public void onScrollChanged(int l, int t, int oldl, int oldt) {
        super.onScrollChanged(l, t, oldl, oldt);
    }

    @Override
    protected void onSizeChanged(int w, int h, int ow, int oh) {
        TraceEvent.begin();
        super.onSizeChanged(w, h, ow, oh);
        mContentViewCore.onSizeChanged(w, h, ow, oh);
        TraceEvent.end();
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        return mContentViewCore.onCreateInputConnection(outAttrs);
    }

    @Override
    public boolean onCheckIsTextEditor() {
        return mContentViewCore.onCheckIsTextEditor();
    }

    @Override
    protected void onFocusChanged(boolean gainFocus, int direction, Rect previouslyFocusedRect) {
        TraceEvent.begin();
        super.onFocusChanged(gainFocus, direction, previouslyFocusedRect);
        mContentViewCore.onFocusChanged(gainFocus, direction, previouslyFocusedRect);
        TraceEvent.end();
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        return mContentViewCore.onKeyUp(keyCode, event);
    }

    @Override
    public boolean dispatchKeyEventPreIme(KeyEvent event) {
        return mContentViewCore.dispatchKeyEventPreIme(event);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        return mContentViewCore.dispatchKeyEvent(event);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return mContentViewCore.onTouchEvent(event);
    }

    /**
     * Mouse move events are sent on hover enter, hover move and hover exit.
     * They are sent on hover exit because sometimes it acts as both a hover
     * move and hover exit.
     */
    @Override
    public boolean onHoverEvent(MotionEvent event) {
        return mContentViewCore.onHoverEvent(event);
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        return mContentViewCore.onGenericMotionEvent(event);
    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        mContentViewCore.onConfigurationChanged(newConfig);
    }

    /**
     * Currently the ContentView scrolling happens in the native side. In
     * the Java view system, it is always pinned at (0, 0). scrollBy() and scrollTo()
     * are overridden, so that View's mScrollX and mScrollY will be unchanged at
     * (0, 0). This is critical for drawing ContentView correctly.
     */
    @Override
    public void scrollBy(int x, int y) {
        mContentViewCore.scrollBy(x, y);
    }

    @Override
    public void scrollTo(int x, int y) {
        mContentViewCore.scrollTo(x, y);
    }

    @Override
    protected int computeHorizontalScrollExtent() {
        // TODO (dtrainor): Need to expose scroll events properly to public. Either make getScroll*
        // work or expose computeHorizontalScrollOffset()/computeVerticalScrollOffset as public.
        return mContentViewCore.computeHorizontalScrollExtent();
    }

    @Override
    protected int computeHorizontalScrollOffset() {
        return mContentViewCore.computeHorizontalScrollOffset();
    }

    @Override
    protected int computeHorizontalScrollRange() {
        return mContentViewCore.computeHorizontalScrollRange();
    }

    @Override
    protected int computeVerticalScrollExtent() {
        return mContentViewCore.computeVerticalScrollExtent();
    }

    @Override
    protected int computeVerticalScrollOffset() {
        return mContentViewCore.computeVerticalScrollOffset();
    }

    @Override
    protected int computeVerticalScrollRange() {
        return mContentViewCore.computeVerticalScrollRange();
    }

    // End FrameLayout overrides.

    @Override
    public boolean awakenScrollBars(int startDelay, boolean invalidate) {
        return mContentViewCore.awakenScrollBars(startDelay, invalidate);
    }

    @Override
    public boolean awakenScrollBars() {
        return super.awakenScrollBars();
    }

    public int getSingleTapX()  {
        return mContentViewCore.getContentViewGestureHandler().getSingleTapX();
    }

    public int getSingleTapY()  {
        return mContentViewCore.getContentViewGestureHandler().getSingleTapY();
    }

    @Override
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        super.onInitializeAccessibilityNodeInfo(info);
        mContentViewCore.onInitializeAccessibilityNodeInfo(info);
    }

    /**
     * Fills in scrolling values for AccessibilityEvents.
     * @param event Event being fired.
     */
    @Override
    public void onInitializeAccessibilityEvent(AccessibilityEvent event) {
        super.onInitializeAccessibilityEvent(event);
        mContentViewCore.onInitializeAccessibilityEvent(event);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        mContentViewCore.onAttachedToWindow();
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mContentViewCore.onDetachedFromWindow();
    }

    @Override
    protected void onVisibilityChanged(View changedView, int visibility) {
        super.onVisibilityChanged(changedView, visibility);
        mContentViewCore.onVisibilityChanged(changedView, visibility);
    }

    void updateMultiTouchZoomSupport() {
        mContentViewCore.updateMultiTouchZoomSupport();
    }

    public boolean isMultiTouchZoomSupported() {
        return mContentViewCore.isMultiTouchZoomSupported();
    }

    /**
     * Register the delegate to be used when content can not be handled by
     * the rendering engine, and should be downloaded instead. This will replace
     * the current delegate.
     * @param delegate An implementation of ContentViewDownloadDelegate.
     */
    public void setDownloadDelegate(ContentViewDownloadDelegate delegate) {
        mContentViewCore.setDownloadDelegate(delegate);
    }

    // Called by DownloadController.
    ContentViewDownloadDelegate getDownloadDelegate() {
        return mContentViewCore.getDownloadDelegate();
    }

    public boolean getUseDesktopUserAgent() {
        return mContentViewCore.getUseDesktopUserAgent();
    }

    /**
     * Set whether or not we're using a desktop user agent for the currently loaded page.
     * @param override If true, use a desktop user agent.  Use a mobile one otherwise.
     * @param reloadOnChange Reload the page if the UA has changed.
     */
    public void setUseDesktopUserAgent(boolean override, boolean reloadOnChange) {
        mContentViewCore.setUseDesktopUserAgent(override, reloadOnChange);
    }

    /**
     * @return Whether the native ContentView has crashed.
     */
    public boolean isCrashed() {
        return mContentViewCore.isCrashed();
    }

    /**
     * @return Whether a reload happens when this ContentView is activated.
     */
    public boolean needsReload() {
        return mContentViewCore.needsReload();
    }

    /**
     * Checks whether the WebView can be zoomed in.
     *
     * @return True if the WebView can be zoomed in.
     */
    // This method uses the term 'zoom' for legacy reasons, but relates
    // to what chrome calls the 'page scale factor'.
    public boolean canZoomIn() {
        return mContentViewCore.canZoomIn();
    }

    /**
     * Checks whether the WebView can be zoomed out.
     *
     * @return True if the WebView can be zoomed out.
     */
    // This method uses the term 'zoom' for legacy reasons, but relates
    // to what chrome calls the 'page scale factor'.
    public boolean canZoomOut() {
        return mContentViewCore.canZoomOut();
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
        return mContentViewCore.zoomIn();
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
        return mContentViewCore.zoomOut();
    }

    /**
     * Resets the zoom factor of the WebView.
     *
     * @return True if there was a zoom change, false otherwise.
     */
    // This method uses the term 'zoom' for legacy reasons, but relates
    // to what chrome calls the 'page scale factor'.
    public boolean zoomReset() {
        return mContentViewCore.zoomReset();
    }

    // Invokes the graphical zoom picker widget for this ContentView.
    public void invokeZoomPicker() {
        mContentViewCore.invokeZoomPicker();
    }

    // Unlike legacy WebView getZoomControls which returns external zoom controls,
    // this method returns built-in zoom controls. This method is used in tests.
    public View getZoomControlsForTest() {
        return mContentViewCore.getZoomControlsForTest();
    }

    /**
     * Return the current scale of the WebView
     * @return The current scale.
     */
    public float getScale() {
        return mContentViewCore.getScale();
    }

    /**
     * If the view is ready to draw contents to the screen. In hardware mode,
     * the initialization of the surface texture may not occur until after the
     * view has been added to the layout. This method will return {@code true}
     * once the texture is actually ready.
     */
    public boolean isReady() {
        return mContentViewCore.isReady();
    }

    /**
     * @return Whether or not the texture view is available or not.
     */
    public boolean isAvailable() {
        return mContentViewCore.isAvailable();
    }

    /**
     * Returns whether or not accessibility injection is being used.
     */
    public boolean isInjectingAccessibilityScript() {
        return mContentViewCore.isInjectingAccessibilityScript();
    }

    /**
     * Enable or disable accessibility features.
     */
    public void setAccessibilityState(boolean state) {
        mContentViewCore.setAccessibilityState(state);
    }

    /**
     * Stop any TTS notifications that are currently going on.
     */
    public void stopCurrentAccessibilityNotifications() {
        mContentViewCore.stopCurrentAccessibilityNotifications();
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //              Start Implementation of ContentViewCore.InternalAccessDelegate               //
    ///////////////////////////////////////////////////////////////////////////////////////////////

    @Override
    public boolean super_onKeyUp(int keyCode, KeyEvent event) {
        return super.onKeyUp(keyCode, event);
    }

    @Override
    public boolean super_dispatchKeyEventPreIme(KeyEvent event) {
        return super.dispatchKeyEventPreIme(event);
    }

    @Override
    public boolean super_dispatchKeyEvent(KeyEvent event) {
        return super.dispatchKeyEvent(event);
    }

    @Override
    public boolean super_onGenericMotionEvent(MotionEvent event) {
        return super.onGenericMotionEvent(event);
    }

    @Override
    public void super_onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
    }

    @Override
    public boolean super_awakenScrollBars(int startDelay, boolean invalidate) {
        return super.awakenScrollBars(startDelay, invalidate);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //                End Implementation of ContentViewCore.InternalAccessDelegate               //
    ///////////////////////////////////////////////////////////////////////////////////////////////
}

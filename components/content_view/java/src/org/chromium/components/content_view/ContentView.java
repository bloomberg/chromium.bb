// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_view;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.view.DragEvent;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewStructure;
import android.view.accessibility.AccessibilityNodeProvider;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.widget.FrameLayout;

import org.chromium.base.TraceEvent;
import org.chromium.content_public.browser.ContentViewCore;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.SmartClipProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.ui.base.EventForwarder;

/**
 * The containing view for {@link WebContents} that exists in the Android UI hierarchy and exposes
 * the various {@link View} functionality to it.
 */
public class ContentView
        extends FrameLayout implements ContentViewCore.InternalAccessDelegate, SmartClipProvider {
    private static final String TAG = "cr.ContentView";

    // Default value to signal that the ContentView's size need not be overridden.
    public static final int DEFAULT_MEASURE_SPEC =
            MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

    private final WebContents mWebContents;
    private EventForwarder mEventForwarder;

    /**
     * The desired size of this view in {@link MeasureSpec}. Set by the host
     * when it should be different from that of the parent.
     */
    private int mDesiredWidthMeasureSpec = DEFAULT_MEASURE_SPEC;
    private int mDesiredHeightMeasureSpec = DEFAULT_MEASURE_SPEC;

    /**
     * Constructs a new ContentView for the appropriate Android version.
     * @param context The Context the view is running in, through which it can
     *                access the current theme, resources, etc.
     * @param webContents The WebContents managing this content view.
     * @return an instance of a ContentView.
     */
    public static ContentView createContentView(Context context, WebContents webContents) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            return new ContentViewApi23(context, webContents);
        }
        return new ContentView(context, webContents);
    }

    /**
     * Creates an instance of a ContentView.
     * @param context The Context the view is running in, through which it can
     *                access the current theme, resources, etc.
     * @param webContents A pointer to the WebContents managing this content view.
     */
    ContentView(Context context, WebContents webContents) {
        super(context, null, android.R.attr.webViewStyle);

        if (getScrollBarStyle() == View.SCROLLBARS_INSIDE_OVERLAY) {
            setHorizontalScrollBarEnabled(false);
            setVerticalScrollBarEnabled(false);
        }

        mWebContents = webContents;

        setFocusable(true);
        setFocusableInTouchMode(true);
    }

    protected WebContentsAccessibility getWebContentsAccessibility() {
        return !mWebContents.isDestroyed() ? WebContentsAccessibility.fromWebContents(mWebContents)
                                           : null;
    }

    @Override
    public boolean performAccessibilityAction(int action, Bundle arguments) {
        WebContentsAccessibility wcax = getWebContentsAccessibility();
        return wcax != null && wcax.supportsAction(action)
                ? wcax.performAction(action, arguments)
                : super.performAccessibilityAction(action, arguments);
    }

    /**
     * Set the desired size of the view. The values are in {@link MeasureSpec}.
     * @param width The width of the content view.
     * @param height The height of the content view.
     */
    public void setDesiredMeasureSpec(int width, int height) {
        mDesiredWidthMeasureSpec = width;
        mDesiredHeightMeasureSpec = height;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (mDesiredWidthMeasureSpec != DEFAULT_MEASURE_SPEC) {
            widthMeasureSpec = mDesiredWidthMeasureSpec;
        }
        if (mDesiredHeightMeasureSpec != DEFAULT_MEASURE_SPEC) {
            heightMeasureSpec = mDesiredHeightMeasureSpec;
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    public AccessibilityNodeProvider getAccessibilityNodeProvider() {
        WebContentsAccessibility wcax = getWebContentsAccessibility();
        AccessibilityNodeProvider provider =
                (wcax != null) ? wcax.getAccessibilityNodeProvider() : null;
        return (provider != null) ? provider : super.getAccessibilityNodeProvider();
    }

    // Needed by ContentViewCore.InternalAccessDelegate
    @Override
    public void onScrollChanged(int l, int t, int oldl, int oldt) {
        super.onScrollChanged(l, t, oldl, oldt);
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        if (getContentViewCore() == null) return null;
        return ImeAdapter.fromWebContents(mWebContents).onCreateInputConnection(outAttrs);
    }

    @Override
    public boolean onCheckIsTextEditor() {
        if (getContentViewCore() == null) return false;

        return ImeAdapter.fromWebContents(mWebContents).onCheckIsTextEditor();
    }

    @Override
    protected void onFocusChanged(boolean gainFocus, int direction, Rect previouslyFocusedRect) {
        try {
            TraceEvent.begin("ContentView.onFocusChanged");
            super.onFocusChanged(gainFocus, direction, previouslyFocusedRect);
            ContentViewCore cvc = getContentViewCore();
            if (cvc != null) {
                cvc.setHideKeyboardOnBlur(true);
                cvc.onViewFocusChanged(gainFocus);
            }
        } finally {
            TraceEvent.end("ContentView.onFocusChanged");
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        super.onWindowFocusChanged(hasWindowFocus);
        ContentViewCore cvc = getContentViewCore();
        if (cvc != null) cvc.onWindowFocusChanged(hasWindowFocus);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        return getEventForwarder().onKeyUp(keyCode, event);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        return isFocused() ? getEventForwarder().dispatchKeyEvent(event)
                           : super.dispatchKeyEvent(event);
    }

    @Override
    public boolean onDragEvent(DragEvent event) {
        return getEventForwarder().onDragEvent(event, this);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return getEventForwarder().onTouchEvent(event);
    }

    /**
     * Mouse move events are sent on hover enter, hover move and hover exit.
     * They are sent on hover exit because sometimes it acts as both a hover
     * move and hover exit.
     */
    @Override
    public boolean onHoverEvent(MotionEvent event) {
        boolean consumed = getEventForwarder().onHoverEvent(event);
        WebContentsAccessibility wcax = getWebContentsAccessibility();
        if (wcax != null && !wcax.isTouchExplorationEnabled()) super.onHoverEvent(event);
        return consumed;
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        return getEventForwarder().onGenericMotionEvent(event);
    }

    private ContentViewCore getContentViewCore() {
        if (mWebContents.isDestroyed()) return null;
        return ContentViewCore.fromWebContents(mWebContents);
    }

    private EventForwarder getEventForwarder() {
        if (mEventForwarder == null) {
            mEventForwarder = mWebContents.getEventForwarder();
        }
        return mEventForwarder;
    }

    @Override
    public boolean performLongClick() {
        return false;
    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        ContentViewCore cvc = getContentViewCore();
        if (cvc != null) cvc.onConfigurationChanged(newConfig);
        super.onConfigurationChanged(newConfig);
    }

    /**
     * Currently the ContentView scrolling happens in the native side. In
     * the Java view system, it is always pinned at (0, 0). scrollBy() and scrollTo()
     * are overridden, so that View's mScrollX and mScrollY will be unchanged at
     * (0, 0). This is critical for drawing ContentView correctly.
     */
    @Override
    public void scrollBy(int x, int y) {
        getEventForwarder().scrollBy(x, y);
    }

    @Override
    public void scrollTo(int x, int y) {
        getEventForwarder().scrollTo(x, y);
    }

    @Override
    protected int computeHorizontalScrollExtent() {
        RenderCoordinates rc = RenderCoordinates.fromWebContents(mWebContents);
        return rc != null ? rc.getLastFrameViewportWidthPixInt() : 0;
    }

    @Override
    protected int computeHorizontalScrollOffset() {
        RenderCoordinates rc = RenderCoordinates.fromWebContents(mWebContents);
        return rc != null ? rc.getScrollXPixInt() : 0;
    }

    @Override
    protected int computeHorizontalScrollRange() {
        RenderCoordinates rc = RenderCoordinates.fromWebContents(mWebContents);
        return rc != null ? rc.getContentWidthPixInt() : 0;
    }

    @Override
    protected int computeVerticalScrollExtent() {
        RenderCoordinates rc = RenderCoordinates.fromWebContents(mWebContents);
        return rc != null ? rc.getLastFrameViewportHeightPixInt() : 0;
    }

    @Override
    protected int computeVerticalScrollOffset() {
        RenderCoordinates rc = RenderCoordinates.fromWebContents(mWebContents);
        return rc != null ? rc.getScrollYPixInt() : 0;
    }

    @Override
    protected int computeVerticalScrollRange() {
        RenderCoordinates rc = RenderCoordinates.fromWebContents(mWebContents);
        return rc != null ? rc.getContentHeightPixInt() : 0;
    }

    // End FrameLayout overrides.

    @Override
    public boolean awakenScrollBars(int startDelay, boolean invalidate) {
        // For the default implementation of ContentView which draws the scrollBars on the native
        // side, calling this function may get us into a bad state where we keep drawing the
        // scrollBars, so disable it by always returning false.
        if (getScrollBarStyle() == View.SCROLLBARS_INSIDE_OVERLAY) return false;
        return super.awakenScrollBars(startDelay, invalidate);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        ContentViewCore cvc = getContentViewCore();
        if (cvc != null) cvc.onAttachedToWindow();
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        ContentViewCore cvc = getContentViewCore();
        if (cvc != null) cvc.onDetachedFromWindow();
    }

    // Implements SmartClipProvider
    @Override
    public void extractSmartClipData(int x, int y, int width, int height) {
        mWebContents.requestSmartClipExtract(x, y, width, height);
    }

    // Implements SmartClipProvider
    @Override
    public void setSmartClipResultHandler(final Handler resultHandler) {
        mWebContents.setSmartClipResultHandler(resultHandler);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //              Start Implementation of ContentViewCore.InternalAccessDelegate               //
    ///////////////////////////////////////////////////////////////////////////////////////////////

    @Override
    public boolean super_onKeyUp(int keyCode, KeyEvent event) {
        return super.onKeyUp(keyCode, event);
    }

    @Override
    public boolean super_dispatchKeyEvent(KeyEvent event) {
        return super.dispatchKeyEvent(event);
    }

    @Override
    public boolean super_onGenericMotionEvent(MotionEvent event) {
        return super.onGenericMotionEvent(event);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    //                End Implementation of ContentViewCore.InternalAccessDelegate               //
    ///////////////////////////////////////////////////////////////////////////////////////////////

    private static class ContentViewApi23 extends ContentView {
        public ContentViewApi23(Context context, WebContents webContents) {
            super(context, webContents);
        }

        @Override
        public void onProvideVirtualStructure(final ViewStructure structure) {
            WebContentsAccessibility wcax = getWebContentsAccessibility();
            if (wcax != null) wcax.onProvideVirtualStructure(structure, false);
        }
    }
}

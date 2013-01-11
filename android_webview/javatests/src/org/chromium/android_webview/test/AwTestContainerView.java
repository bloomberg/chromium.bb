// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.util.Log;

import org.chromium.android_webview.AwContents;
import org.chromium.content.browser.ContentViewCore;

/**
 * A View used for testing the AwContents internals.
 *
 * This class takes the place android.webkit.WebView would have in the production configuration.
 */
class AwTestContainerView extends FrameLayout {
    private AwContents mAwContents;
    private AwContents.InternalAccessDelegate mInternalAccessDelegate;

    public AwTestContainerView(Context context) {
        super(context);
        mInternalAccessDelegate = new InternalAccessAdapter();
    }

    public void initialize(AwContents awContents) {
        mAwContents = awContents;
    }

    public ContentViewCore getContentViewCore() {
        return mAwContents.getContentViewCore();
    }

    public AwContents getAwContents() {
        return mAwContents;
    }

    public AwContents.InternalAccessDelegate getInternalAccessDelegate() {
        return mInternalAccessDelegate;
    }

    public void destroy() {
        mAwContents.destroy();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        mAwContents.onConfigurationChanged(newConfig);
    }

    @Override
    public void onAttachedToWindow() {
        mAwContents.onAttachedToWindow();
    }

    @Override
    public void onDetachedFromWindow() {
        mAwContents.onDetachedFromWindow();
    }

    @Override
    public void onFocusChanged(boolean focused, int direction, Rect previouslyFocusedRect) {
        mAwContents.onFocusChanged(focused, direction, previouslyFocusedRect);
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        mAwContents.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    public void onSizeChanged(int w, int h, int ow, int oh) {
        mAwContents.onSizeChanged(w, h, ow, oh);
    }

    @Override
    public void onVisibilityChanged(View changedView, int visibility) {
        mAwContents.onVisibilityChanged(changedView, visibility);
    }

    @Override
    public void onWindowVisibilityChanged(int visibility) {
        mAwContents.onWindowVisibilityChanged(visibility);
    }

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        return mAwContents.onTouchEvent(ev);
    }

    @Override
    public void onDraw(Canvas canvas) {
        mAwContents.onDraw(canvas);
    }

    // TODO: AwContents could define a generic class that holds an implementation similar to
    // the one below.
    private class InternalAccessAdapter implements AwContents.InternalAccessDelegate {

        @Override
        public boolean drawChild(Canvas canvas, View child, long drawingTime) {
            return AwTestContainerView.super.drawChild(canvas, child, drawingTime);
        }

        @Override
        public boolean super_onKeyUp(int keyCode, KeyEvent event) {
            return AwTestContainerView.super.onKeyUp(keyCode, event);
        }

        @Override
        public boolean super_dispatchKeyEventPreIme(KeyEvent event) {
            return AwTestContainerView.super.dispatchKeyEventPreIme(event);
        }

        @Override
        public boolean super_dispatchKeyEvent(KeyEvent event) {
            return AwTestContainerView.super.dispatchKeyEvent(event);
        }

        @Override
        public boolean super_onGenericMotionEvent(MotionEvent event) {
            return AwTestContainerView.super.onGenericMotionEvent(event);
        }

        @Override
        public void super_onConfigurationChanged(Configuration newConfig) {
            AwTestContainerView.super.onConfigurationChanged(newConfig);
        }

        @Override
        public void onScrollChanged(int l, int t, int oldl, int oldt) {
            AwTestContainerView.super.onScrollChanged(l, t, oldl, oldt);
        }

        @Override
        public boolean awakenScrollBars() {
            return AwTestContainerView.super.awakenScrollBars();
        }

        @Override
        public boolean super_awakenScrollBars(int startDelay, boolean invalidate) {
            return AwTestContainerView.super.awakenScrollBars(startDelay, invalidate);
        }

        @Override
        public void setMeasuredDimension(int measuredWidth, int measuredHeight) {
            AwTestContainerView.super.setMeasuredDimension(measuredWidth, measuredHeight);
        }
    }
}

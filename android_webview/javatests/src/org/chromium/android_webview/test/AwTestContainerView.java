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
    private ContentViewCore mContentViewCore;
    private AwContents mAwContents;
    private ContentViewCore.InternalAccessDelegate mInternalAccessDelegate;

    public AwTestContainerView(Context context) {
        super(context);
        mInternalAccessDelegate = new InternalAccessAdapter();
    }

    public void initialize(ContentViewCore contentViewCore, AwContents awContents) {
        mContentViewCore = contentViewCore;
        mAwContents = awContents;
    }

    public ContentViewCore getContentViewCore() {
        return mContentViewCore;
    }

    public AwContents getAwContents() {
        return mAwContents;
    }

    public ContentViewCore.InternalAccessDelegate getInternalAccessDelegate() {
        return mInternalAccessDelegate;
    }

    public void destroy() {
        mAwContents.destroy();
    }

    // TODO: Required ContentViewCore changes are not upstreamed yet.
    /*
    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        mContentViewCore.onConfigurationChanged(newConfig);
    }

    @Override
    public void onAttachedToWindow() {
        mContentViewCore.onAttachedToWindow();
    }

    @Override
    public void onDetachedFromWindow() {
        mContentViewCore.onDetachedFromWindow();
    }

    @Override
    public void onFocusChanged(boolean focused, int direction, Rect previouslyFocusedRect) {
        mContentViewCore.onFocusChanged(focused, direction, previouslyFocusedRect);
    }

    @Override
    public void onSizeChanged(int w, int h, int ow, int oh) {
        mContentViewCore.onSizeChanged(w, h, ow, oh);
    }
    */

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        return mContentViewCore.onTouchEvent(ev);
    }

    // TODO: ContentViewCore could define a generic class that holds an implementation similar to
    // the one below.
    private class InternalAccessAdapter implements ContentViewCore.InternalAccessDelegate {

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

        // TODO: Required ContentViewCore changes are not upstreamed yet.
        /*
        @Override
        public void onSurfaceTextureUpdated() {
        }
        */
    }
}

// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Canvas;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeProvider;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.widget.FrameLayout;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.shell.DrawGL;
import org.chromium.content.browser.ContentViewCore;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * A View used for testing the AwContents internals.
 *
 * This class takes the place android.webkit.WebView would have in the production configuration.
 */
public class AwTestContainerView extends FrameLayout {
    private AwContents mAwContents;
    private AwContents.NativeGLDelegate mNativeGLDelegate;
    private AwContents.InternalAccessDelegate mInternalAccessDelegate;

    HardwareView mHardwareView = null;
    private boolean mAttachedContents = false;

    private class HardwareView extends GLSurfaceView {
        private static final int MODE_DRAW = 0;
        private static final int MODE_PROCESS = 1;
        private static final int MODE_PROCESS_NO_CONTEXT = 2;
        private static final int MODE_SYNC = 3;

        // mSyncLock is used to synchronized requestRender on the UI thread
        // and drawGL on the rendering thread. The variables following
        // are protected by it.
        private final Object mSyncLock = new Object();
        private boolean mFunctorAttached = false;
        private boolean mNeedsProcessGL = false;
        private boolean mNeedsDrawGL = false;
        private boolean mWaitForCompletion = false;
        private int mLastScrollX = 0;
        private int mLastScrollY = 0;

        private int mCommittedScrollX = 0;
        private int mCommittedScrollY = 0;

        private boolean mHaveSurface = false;
        private Runnable mReadyToRenderCallback = null;

        private long mDrawGL = 0;
        private long mViewContext = 0;

        public HardwareView(Context context) {
            super(context);
            setEGLContextClientVersion(2); // GLES2
            getHolder().setFormat(PixelFormat.OPAQUE);
            setPreserveEGLContextOnPause(true);
            setRenderer(new Renderer() {
                private int mWidth = 0;
                private int mHeight = 0;

                @Override
                public void onDrawFrame(GL10 gl) {
                    HardwareView.this.drawGL(mWidth, mHeight);
                }

                @Override
                public void onSurfaceChanged(GL10 gl, int width, int height) {
                    gl.glViewport(0, 0, width, height);
                    gl.glScissor(0, 0, width, height);
                    mWidth = width;
                    mHeight = height;
                }

                @Override
                public void onSurfaceCreated(GL10 gl, EGLConfig config) {
                }
            });

            setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
        }

        public void initialize(long drawGL, long viewContext) {
            mDrawGL = drawGL;
            mViewContext = viewContext;
        }

        public boolean isReadyToRender() {
            return mHaveSurface;
        }

        public void setReadyToRenderCallback(Runnable runner) {
            assert !isReadyToRender() || runner == null;
            mReadyToRenderCallback = runner;
        }

        @Override
        public void surfaceCreated(SurfaceHolder holder) {
            boolean didHaveSurface = mHaveSurface;
            mHaveSurface = true;
            if (!didHaveSurface && mReadyToRenderCallback != null) {
                mReadyToRenderCallback.run();
                mReadyToRenderCallback = null;
            }
            super.surfaceCreated(holder);
        }

        @Override
        public void surfaceDestroyed(SurfaceHolder holder) {
            mHaveSurface = false;
            super.surfaceDestroyed(holder);
        }

        public void updateScroll(int x, int y) {
            synchronized (mSyncLock) {
                mLastScrollX = x;
                mLastScrollY = y;
            }
        }

        public void detachGLFunctor() {
            synchronized (mSyncLock) {
                mFunctorAttached = false;
                mNeedsProcessGL = false;
                mNeedsDrawGL = false;
                mWaitForCompletion = false;
            }
        }

        public void requestRender(Canvas canvas, boolean waitForCompletion) {
            synchronized (mSyncLock) {
                super.requestRender();
                mFunctorAttached = true;
                mWaitForCompletion = waitForCompletion;
                if (canvas == null) {
                    mNeedsProcessGL = true;
                } else {
                    mNeedsDrawGL = true;
                    if (!waitForCompletion) {
                        // Wait until SYNC is complete only.
                        // Do this every time there was a new frame.
                        try {
                            while (mNeedsDrawGL) {
                                mSyncLock.wait();
                            }
                        } catch (InterruptedException e) {
                            // ...
                        }
                    }
                }
                if (waitForCompletion) {
                    try {
                        while (mWaitForCompletion) {
                            mSyncLock.wait();
                        }
                    } catch (InterruptedException e) {
                        // ...
                    }
                }
            }
        }

        public void drawGL(int width, int height) {
            final boolean draw;
            final boolean process;
            final boolean waitForCompletion;

            synchronized (mSyncLock) {
                if (!mFunctorAttached) {
                    mSyncLock.notifyAll();
                    return;
                }

                draw = mNeedsDrawGL;
                process = mNeedsProcessGL;
                waitForCompletion = mWaitForCompletion;

                if (draw) {
                    DrawGL.drawGL(mDrawGL, mViewContext, width, height, 0, 0, MODE_SYNC);
                    mCommittedScrollX = mLastScrollX;
                    mCommittedScrollY = mLastScrollY;
                }
                mNeedsDrawGL = false;
                mNeedsProcessGL = false;
                if (!waitForCompletion) {
                    mSyncLock.notifyAll();
                }
            }
            if (draw) {
                DrawGL.drawGL(mDrawGL, mViewContext, width, height,
                        mCommittedScrollX, mCommittedScrollY, MODE_DRAW);
            } else if (process) {
                DrawGL.drawGL(mDrawGL, mViewContext, width, height, 0, 0, MODE_PROCESS);
            }

            if (waitForCompletion) {
                synchronized (mSyncLock) {
                    mWaitForCompletion = false;
                    mSyncLock.notifyAll();
                }
            }
        }
    }

    private static boolean sCreatedOnce = false;
    private HardwareView createHardwareViewOnlyOnce(Context context) {
        if (sCreatedOnce) return null;
        sCreatedOnce = true;
        return new HardwareView(context);
    }

    public AwTestContainerView(Context context, boolean hardwareAccelerated) {
        super(context);
        if (hardwareAccelerated) {
            mHardwareView = createHardwareViewOnlyOnce(context);
        }
        if (mHardwareView != null) {
            addView(mHardwareView,
                    new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));
        } else {
          setLayerType(LAYER_TYPE_SOFTWARE, null);
        }
        mNativeGLDelegate = new NativeGLDelegate();
        mInternalAccessDelegate = new InternalAccessAdapter();
        setOverScrollMode(View.OVER_SCROLL_ALWAYS);
        setFocusable(true);
        setFocusableInTouchMode(true);
    }

    public void initialize(AwContents awContents) {
        mAwContents = awContents;
        if (mHardwareView != null) {
            mHardwareView.initialize(
                    mAwContents.getAwDrawGLFunction(), mAwContents.getAwDrawGLViewContext());
        }
    }

    public ContentViewCore getContentViewCore() {
        return mAwContents.getContentViewCore();
    }

    public AwContents getAwContents() {
        return mAwContents;
    }

    public AwContents.NativeGLDelegate getNativeGLDelegate() {
        return mNativeGLDelegate;
    }

    public AwContents.InternalAccessDelegate getInternalAccessDelegate() {
        return mInternalAccessDelegate;
    }

    public void destroy() {
        mAwContents.destroy();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        mAwContents.onConfigurationChanged(newConfig);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (mHardwareView == null || mHardwareView.isReadyToRender()) {
            mAwContents.onAttachedToWindow();
            mAttachedContents = true;
        } else {
            mHardwareView.setReadyToRenderCallback(new Runnable() {
                public void run() {
                    assert !mAttachedContents;
                    mAwContents.onAttachedToWindow();
                    mAttachedContents = true;
                }
            });
        }
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mAwContents.onDetachedFromWindow();
        if (mHardwareView != null) {
            mHardwareView.setReadyToRenderCallback(null);
        }
        mAttachedContents = false;
    }

    @Override
    public void onFocusChanged(boolean focused, int direction, Rect previouslyFocusedRect) {
        super.onFocusChanged(focused, direction, previouslyFocusedRect);
        mAwContents.onFocusChanged(focused, direction, previouslyFocusedRect);
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        return mAwContents.onCreateInputConnection(outAttrs);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        return mAwContents.onKeyUp(keyCode, event);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        return mAwContents.dispatchKeyEvent(event);
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        mAwContents.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    public void onSizeChanged(int w, int h, int ow, int oh) {
        super.onSizeChanged(w, h, ow, oh);
        mAwContents.onSizeChanged(w, h, ow, oh);
    }

    @Override
    public void onOverScrolled(int scrollX, int scrollY, boolean clampedX, boolean clampedY) {
        mAwContents.onContainerViewOverScrolled(scrollX, scrollY, clampedX, clampedY);
    }

    @Override
    public void onScrollChanged(int l, int t, int oldl, int oldt) {
        super.onScrollChanged(l, t, oldl, oldt);
        if (mAwContents != null) {
            mAwContents.onContainerViewScrollChanged(l, t, oldl, oldt);
        }
    }

    @Override
    public void computeScroll() {
        mAwContents.computeScroll();
    }

    @Override
    public void onVisibilityChanged(View changedView, int visibility) {
        super.onVisibilityChanged(changedView, visibility);
        mAwContents.onVisibilityChanged(changedView, visibility);
    }

    @Override
    public void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);
        mAwContents.onWindowVisibilityChanged(visibility);
    }

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        super.onTouchEvent(ev);
        return mAwContents.onTouchEvent(ev);
    }

    @Override
    public void onDraw(Canvas canvas) {
        if (mHardwareView != null) {
            mHardwareView.updateScroll(getScrollX(), getScrollY());
        }
        mAwContents.onDraw(canvas);
        super.onDraw(canvas);
    }

    @Override
    public AccessibilityNodeProvider getAccessibilityNodeProvider() {
        AccessibilityNodeProvider provider =
            mAwContents.getAccessibilityNodeProvider();
        return provider == null ? super.getAccessibilityNodeProvider() : provider;
    }

    @Override
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        super.onInitializeAccessibilityNodeInfo(info);
        info.setClassName(AwContents.class.getName());
        mAwContents.onInitializeAccessibilityNodeInfo(info);
    }

    @Override
    public void onInitializeAccessibilityEvent(AccessibilityEvent event) {
        super.onInitializeAccessibilityEvent(event);
        event.setClassName(AwContents.class.getName());
        mAwContents.onInitializeAccessibilityEvent(event);
    }

    @Override
    public boolean performAccessibilityAction(int action, Bundle arguments) {
        return mAwContents.performAccessibilityAction(action, arguments);
    }

    private class NativeGLDelegate implements AwContents.NativeGLDelegate {
        @Override
        public boolean requestDrawGL(Canvas canvas, boolean waitForCompletion,
                View containerview) {
            if (mHardwareView == null) return false;
            mHardwareView.requestRender(canvas, waitForCompletion);
            return true;
        }

        @Override
        public void detachGLFunctor() {
            if (mHardwareView != null) mHardwareView.detachGLFunctor();
        }
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
        public void super_scrollTo(int scrollX, int scrollY) {
            // We're intentionally not calling super.scrollTo here to make testing easier.
            AwTestContainerView.this.scrollTo(scrollX, scrollY);
            if (mHardwareView != null) {
                // Undo the scroll that will be applied because of mHardwareView
                // being a child of |this|.
                mHardwareView.setTranslationX(scrollX);
                mHardwareView.setTranslationY(scrollY);
            }
        }

        @Override
        public void overScrollBy(int deltaX, int deltaY,
                int scrollX, int scrollY,
                int scrollRangeX, int scrollRangeY,
                int maxOverScrollX, int maxOverScrollY,
                boolean isTouchEvent) {
            // We're intentionally not calling super.scrollTo here to make testing easier.
            AwTestContainerView.this.overScrollBy(deltaX, deltaY, scrollX, scrollY,
                     scrollRangeX, scrollRangeY, maxOverScrollX, maxOverScrollY, isTouchEvent);
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

        @Override
        public int super_getScrollBarStyle() {
            return AwTestContainerView.super.getScrollBarStyle();
        }
    }
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.testshell;

import android.util.AttributeSet;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.ViewGroup;
import android.content.Context;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import org.chromium.base.JNINamespace;

import org.chromium.chrome.browser.ChromeWebContentsDelegateAndroid;
import org.chromium.chrome.browser.TabBase;
import org.chromium.ui.gfx.NativeWindow;

/**
 * The TabManager hooks together all of the related {@link View}s that are used to represent
 * a {@link TabBase}.  It properly builds a {@link TabBase} and makes sure that the {@link Toolbar}
 * and {@link SurfaceView} show the proper content.
 */
@JNINamespace("chrome")
public class TabManager extends LinearLayout {
    private static final String DEFAULT_URL = "http://www.google.com";

    private NativeWindow mWindow;
    private ViewGroup mContentViewHolder;
    private SurfaceView mRenderTarget;
    private TestShellToolbar mToolbar;

    private TabBase mCurrentTab;

    /**
     * @param context The Context the view is running in.
     * @param attrs   The attributes of the XML tag that is inflating the view.
     */
    public TabManager(Context context, AttributeSet attrs) {
        super(context, attrs);
        nativeInit(this);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mContentViewHolder = (ViewGroup) findViewById(R.id.content_container);
        mToolbar = (TestShellToolbar) findViewById(R.id.toolbar);
        mRenderTarget = (SurfaceView) findViewById(R.id.render_target);
        mRenderTarget.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                nativeSurfaceDestroyed();
            }

            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                nativeSurfaceCreated(holder.getSurface());

                if (mCurrentTab == null) createTab(DEFAULT_URL);
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                nativeSurfaceSetSize(width, height);
            }
        });
    }

    /**
     * @param window The window used to generate all ContentViews.
     */
    public void setWindow(NativeWindow window) {
        mWindow = window;
    }

    /**
     * @return The currently visible {@link TabBase}.
     */
    public TabBase getCurrentTab() {
        return mCurrentTab;
    }

    /**
     * Creates a {@link TabBase} with a URL specified by {@code url}.
     * @param url The URL the new {@link TabBase} should start with.
     */
    public void createTab(String url) {
        if (!isRenderTargetInitialized()) return;

        TabBase tab = new TabBase(getContext(), url, mWindow,
                new TabManagerWebContentsDelegateAndroid());
        setCurrentTab(tab);
    }

    private boolean isRenderTargetInitialized() {
        return mRenderTarget != null && mRenderTarget.getHolder().getSurface() != null;
    }

    private void setCurrentTab(TabBase tab) {
        if (mCurrentTab != null) {
            mContentViewHolder.removeView(mCurrentTab.getContentView());
            nativeHideTab(mCurrentTab.getNativeTab());
            mCurrentTab.destroy();
        }

        mCurrentTab = tab;

        mToolbar.showTab(mCurrentTab);
        mContentViewHolder.addView(mCurrentTab.getContentView());
        mCurrentTab.getContentView().requestFocus();
        nativeShowTab(mCurrentTab.getNativeTab());
    }

    private class TabManagerWebContentsDelegateAndroid extends ChromeWebContentsDelegateAndroid {
        @Override
        public void onLoadProgressChanged(int progress) {
            mToolbar.onLoadProgressChanged(progress);
        }

        @Override
        public void onUpdateUrl(String url) {
            mToolbar.onUpdateUrl(url);
        }
    }

    private static native void nativeShowTab(int jtab);
    private static native void nativeHideTab(int jtab);
    private static native void nativeInit(Object renderTargetInstance);
    private static native void nativeSurfaceCreated(Surface surface);
    private static native void nativeSurfaceDestroyed();
    private static native void nativeSurfaceSetSize(int width, int height);
}
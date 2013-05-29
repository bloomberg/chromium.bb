// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.FrameLayout;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.ContentViewRenderView;
import org.chromium.ui.WindowAndroid;

/**
 * Container and generator of ShellViews.
 */
@JNINamespace("content")
public class ShellManager extends FrameLayout {

    public static final String DEFAULT_SHELL_URL = "http://www.google.com";
    private static boolean sStartup = true;
    private WindowAndroid mWindow;
    private Shell mActiveShell;

    private String mStartupUrl = DEFAULT_SHELL_URL;

    // The target for all content rendering.
    private ContentViewRenderView mContentViewRenderView;

    /**
     * Constructor for inflating via XML.
     */
    public ShellManager(Context context, AttributeSet attrs) {
        super(context, attrs);
        nativeInit(this);
        mContentViewRenderView = new ContentViewRenderView(context) {
            @Override
            protected void onReadyToRender() {
                if (sStartup) {
                    mActiveShell.loadUrl(mStartupUrl);
                    sStartup = false;
                }
            }
        };
    }

    /**
     * @param window The window used to generate all shells.
     */
    public void setWindow(WindowAndroid window) {
        mWindow = window;
    }

    /**
     * @return The window used to generate all shells.
     */
    public WindowAndroid getWindow() {
        return mWindow;
    }

    /**
     * Sets the startup URL for new shell windows.
     */
    public void setStartupUrl(String url) {
        mStartupUrl = url;
    }

    /**
     * @return The currently visible shell view or null if one is not showing.
     */
    public Shell getActiveShell() {
        return mActiveShell;
    }

    /**
     * Creates a new shell pointing to the specified URL.
     * @param url The URL the shell should load upon creation.
     */
    public void launchShell(String url) {
        nativeLaunchShell(url);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private Object createShell() {
        LayoutInflater inflater =
                (LayoutInflater) getContext().getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        Shell shellView = (Shell) inflater.inflate(R.layout.shell_view, null);
        shellView.setWindow(mWindow);

        if (mActiveShell != null) closeShell(mActiveShell);

        shellView.setContentViewRenderView(mContentViewRenderView);
        addView(shellView, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
        mActiveShell = shellView;
        ContentView contentView = mActiveShell.getContentView();
        if (contentView != null) {
            mContentViewRenderView.setCurrentContentView(contentView);
            contentView.onShow();
        }

        return shellView;
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void closeShell(Shell shellView) {
        if (shellView == mActiveShell) mActiveShell = null;
        ContentView contentView = shellView.getContentView();
        if (contentView != null) contentView.onHide();
        shellView.setContentViewRenderView(null);
        shellView.setWindow(null);
        removeView(shellView);
    }

    private static native void nativeInit(Object shellManagerInstance);
    private static native void nativeLaunchShell(String url);
}

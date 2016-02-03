// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.Context;
import android.graphics.Color;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.FrameLayout;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.ContentViewRenderView;
import org.chromium.ui.base.WindowAndroid;

/**
 * Container and generator of CastWindow instances.
 */
@JNINamespace("chromecast::shell")
public class CastWindowManager extends FrameLayout {
    private static final String TAG = "CastWindowManager";

    private WindowAndroid mWindow;
    private CastWindowAndroid mActiveCastWindow;

    // The target for all content rendering.
    private ContentViewRenderView mContentViewRenderView;

    /**
     * Delegate to deliver events from the native window.
     */
    public interface Delegate {
        public void onCreated();
        public void onClosed();
    }
    private Delegate mDelegate;

    /**
     * Constructor for inflating via XML.
     */
    public CastWindowManager(Context context, AttributeSet attrs) {
        super(context, attrs);
        nativeInit(this);
    }

    /**
     * @param delegate Delegate to handle events.
     */
    public void setDelegate(Delegate delegate) {
        mDelegate = delegate;
    }

    /**
     * @param window Represents the activity window.
     */
    public void setWindow(WindowAndroid window) {
        assert window != null;
        mWindow = window;
        mContentViewRenderView = new ContentViewRenderView(getContext()) {
            @Override
            protected void onReadyToRender() {
                setOverlayVideoMode(true);
            }
        };
        mContentViewRenderView.onNativeLibraryLoaded(window);
        // Setting the background color to black avoids rendering a white splash screen
        // before the players are loaded. See crbug/307113 for details.
        mContentViewRenderView.setSurfaceViewBackgroundColor(Color.BLACK);
    }

    /**
     * @return The window used to generate all shells.
     */
    public WindowAndroid getWindow() {
        return mWindow;
    }

    /**
     * @return The currently visible shell view or null if one is not showing.
     */
    public CastWindowAndroid getActiveCastWindow() {
        return mActiveCastWindow;
    }

    /**
     * Creates a new shell pointing to the specified URL.
     * @param url The URL the shell should load upon creation.
     * @return Pointer of native cast shell instance.
     */
    public long launchCastWindow(String url) {
        return nativeLaunchCastWindow(url);
    }

    /**
     * Stops a native cast shell instance created by {@link #launchCastWindow(String)}.
     * @param nativeCastWindow Pointer of native cast shell instance returned
     *        by {@link #launchCastWindow(String)}.
     * @param gracefully Whether or not to call RVH::ClosePage to deliver unload event.
     * @see #launchCastWindow(String)
     */
    public void stopCastWindow(long nativeCastWindow, boolean gracefully) {
        nativeStopCastWindow(nativeCastWindow, gracefully);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private Object createCastWindow() {
        assert mContentViewRenderView != null;
        LayoutInflater inflater =
                (LayoutInflater) getContext().getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        CastWindowAndroid shellView =
                (CastWindowAndroid) inflater.inflate(R.layout.cast_window_view, null);
        shellView.setWindow(mWindow);

        if (mActiveCastWindow != null) closeCastWindow(mActiveCastWindow);

        shellView.setContentViewRenderView(mContentViewRenderView);
        addView(shellView, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
        mActiveCastWindow = shellView;
        ContentViewCore contentViewCore = mActiveCastWindow.getContentViewCore();
        if (contentViewCore != null) {
            mContentViewRenderView.setCurrentContentViewCore(contentViewCore);
            contentViewCore.onShow();
        }

        if (mDelegate != null) {
            mDelegate.onCreated();
        }

        return shellView;
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void closeCastWindow(CastWindowAndroid shellView) {
        if (shellView == mActiveCastWindow) mActiveCastWindow = null;
        ContentViewCore contentViewCore = shellView.getContentViewCore();
        if (contentViewCore != null) contentViewCore.onHide();
        shellView.setContentViewRenderView(null);
        shellView.setWindow(null);
        removeView(shellView);
        mContentViewRenderView.destroy();
        mContentViewRenderView = null;
        if (mDelegate != null) {
            mDelegate.onClosed();
        }
    }

    private static native void nativeInit(Object shellManagerInstance);
    private static native long nativeLaunchCastWindow(String url);
    private static native void nativeStopCastWindow(long pointerOfNativeCastWindow,
            boolean gracefully);
    public static native void nativeEnableDevTools(boolean enable);
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.SurfaceHolder;
import android.widget.FrameLayout;

import org.chromium.base.JNINamespace;

/***
 * This view is used by a ContentView to render its content.
 * It renders the layers added to it through the native ContentViewLayerRenderer to its SurfaceView.
 * TODO(jcivelli): the API is confusing and complicated. There should be a way to do all that
 * wihtout native code.
 */
@JNINamespace("content")
public class ContentViewRenderView extends FrameLayout {

    private SurfaceView mSurfaceView;

    /**
     * Constructs a new ContentViewRenderView that should be can to a view hierarchy.
     * Native code should add/remove the layers to be rendered through the ContentViewLayerRenderer.
     * @param context The context used to create this.
     */
    public ContentViewRenderView(Context context) {
        super(context);

        nativeInit();

        mSurfaceView = new SurfaceView(getContext());
        mSurfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                nativeSurfaceSetSize(width, height);
            }

            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                nativeSurfaceCreated(holder.getSurface());
                onReadyToRender();
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                nativeSurfaceDestroyed();
            }
        });

        addView(mSurfaceView,
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));
    }

    /**
     * @return pointer to a native ContentViewLayerRenderer on which the layers to be added should
     * be added removed.
     */
    public int getNativeContentViewLayerRenderer() {
        return nativeGetNativeContentViewLayerRenderer();
    }

    /**
     * This method should be subclassed to provide actions to be performed once the view is ready to
     * render.
     */
    protected void onReadyToRender() {
    }

    /**
     * @return whether the surface view is initialized and ready to render.
     */
    public boolean isInitialized() {
        return mSurfaceView.getHolder().getSurface() != null;
    }

    private native void nativeInit();
    private static native int nativeGetNativeContentViewLayerRenderer();
    private static native void nativeSurfaceCreated(Surface surface);
    private static native void nativeSurfaceDestroyed();
    private static native void nativeSurfaceSetSize(int width, int height);
}


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
 * Call {@link #setCurrentContentView(ContentView)} with the contentView that should be displayed.
 * Note that only one ContentView can be shown at a time.
 */
@JNINamespace("content")
public class ContentViewRenderView extends FrameLayout {

    // The native side of this object.
    private int mNativeContentViewRenderView = 0;

    private SurfaceView mSurfaceView;

    private ContentView mCurrentContentView;

    /**
     * Constructs a new ContentViewRenderView that should be can to a view hierarchy.
     * Native code should add/remove the layers to be rendered through the ContentViewLayerRenderer.
     * @param context The context used to create this.
     */
    public ContentViewRenderView(Context context) {
        super(context);

        mNativeContentViewRenderView = nativeInit();
        assert mNativeContentViewRenderView != 0;

        mSurfaceView = new SurfaceView(getContext());
        mSurfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                nativeSurfaceSetSize(mNativeContentViewRenderView, width, height);
                if (mCurrentContentView != null) {
                    mCurrentContentView.getContentViewCore().onPhysicalBackingSizeChanged(
                            width, height);
                }
            }

            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                nativeSurfaceCreated(mNativeContentViewRenderView, holder.getSurface());
                onReadyToRender();
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                nativeSurfaceDestroyed(mNativeContentViewRenderView);
            }
        });

        addView(mSurfaceView,
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));
    }

    /**
     * Should be called when the ContentViewRenderView is not needed anymore so its associated
     * native resource can be freed.
     */
    public void destroy() {
        nativeDestroy(mNativeContentViewRenderView);
    }

    /**
     * Makes the passed ContentView the one displayed by this ContentViewRenderView.
     */
    public void setCurrentContentView(ContentView contentView) {
        nativeSetCurrentContentView(mNativeContentViewRenderView,
                contentView.getContentViewCore().getNativeContentViewCore());

        mCurrentContentView = contentView;
        mCurrentContentView.getContentViewCore().onPhysicalBackingSizeChanged(
                getWidth(), getHeight());
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

    private static native int nativeInit();
    private native void nativeDestroy(int nativeContentViewRenderView);
    private native void nativeSetCurrentContentView(int nativeContentViewRenderView,
            int nativeContentView);
    private native void nativeSurfaceCreated(int nativeContentViewRenderView, Surface surface);
    private native void nativeSurfaceDestroyed(int nativeContentViewRenderView);
    private native void nativeSurfaceSetSize(int nativeContentViewRenderView,
            int width, int height);
}

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.graphics.drawable.Drawable;
import android.support.annotation.IntDef;
import android.view.Surface;

import org.chromium.chrome.browser.compositor.CompositorSurfaceManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Provides the texture-backed surface used for drawing Android UI in VR to the CompositorView.
 * This class only partially fulfills the contract that the CompositorSurfaceManagerImpl fulfills.
 *
 * This class doesn't use SurfaceViews, as the texture-backed surface is drawn using GL during VR
 * scene compositing. This class also doesn't really manage the Surface in any way, and fakes the
 * standard Surface/View lifecycle so that the compositor remains unaware of this. The surface
 * format requested by the compositor is also ignored.
 */
public class VrCompositorSurfaceManager implements CompositorSurfaceManager {
    private static final int SURFACE_NOT_REQUESTED = 0;
    private static final int SURFACE_REQUESTED = 1;
    private static final int SURFACE_PROVIDED = 2;

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({SURFACE_NOT_REQUESTED, SURFACE_REQUESTED, SURFACE_PROVIDED})
    private @interface SurfaceState {}

    private @SurfaceState int mSurfaceState = SURFACE_NOT_REQUESTED;
    private Surface mSurface;
    private int mFormat;
    private int mWidth;
    private int mHeight;

    // Client that we notify about surface change events.
    private SurfaceManagerCallbackTarget mClient;

    public VrCompositorSurfaceManager(SurfaceManagerCallbackTarget client) {
        mClient = client;
    }

    /* package */ void setSurface(Surface surface, int format, int width, int height) {
        if (mSurfaceState == SURFACE_PROVIDED) shutDown();
        mSurface = surface;
        mFormat = format;
        mWidth = width;
        mHeight = height;
        if (mSurfaceState == SURFACE_REQUESTED) {
            mClient.surfaceCreated(mSurface);
            mClient.surfaceChanged(mSurface, mFormat, mWidth, mHeight);
            mSurfaceState = SURFACE_PROVIDED;
        }
    }

    /* package */ void surfaceResized(int width, int height) {
        assert mSurface != null;
        mWidth = width;
        mHeight = height;
        if (mSurfaceState == SURFACE_PROVIDED) {
            mClient.surfaceChanged(mSurface, mFormat, mWidth, mHeight);
        }
    }

    /* package */ void surfaceDestroyed() {
        shutDown();
        mSurface = null;
    }

    @Override
    public void shutDown() {
        if (mSurfaceState == SURFACE_PROVIDED) mClient.surfaceDestroyed(mSurface);
        mSurfaceState = SURFACE_NOT_REQUESTED;
    }

    @Override
    public void requestSurface(int format) {
        if (mSurface == null) {
            mSurfaceState = SURFACE_REQUESTED;
            return;
        }
        if (mSurfaceState == SURFACE_PROVIDED) shutDown();
        mClient.surfaceCreated(mSurface);
        mClient.surfaceChanged(mSurface, mFormat, mWidth, mHeight);
        mSurfaceState = SURFACE_PROVIDED;
    }

    @Override
    public void doneWithUnownedSurface() {}

    @Override
    public void recreateSurfaceForJellyBean() {}

    @Override
    public void setBackgroundDrawable(Drawable background) {}

    @Override
    public void setWillNotDraw(boolean willNotDraw) {}

    @Override
    public void setVisibility(int visibility) {}
}

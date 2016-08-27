// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static android.opengl.GLES20.GL_NEAREST;
import static android.opengl.GLES20.GL_TEXTURE_MAG_FILTER;
import static android.opengl.GLES20.GL_TEXTURE_MIN_FILTER;
import static android.opengl.GLES20.glBindTexture;
import static android.opengl.GLES20.glGenTextures;
import static android.opengl.GLES20.glTexParameteri;

import android.app.Activity;
import android.graphics.SurfaceTexture;
import android.graphics.SurfaceTexture.OnFrameAvailableListener;
import android.opengl.GLES11Ext;
import android.opengl.GLSurfaceView;
import android.view.MotionEvent;

import com.google.vr.ndk.base.AndroidCompat;
import com.google.vr.ndk.base.GvrLayout;

import org.chromium.base.annotations.JNINamespace;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * This view extends from GvrLayout which wraps a GLSurfaceView that renders VR shell.
 */
@JNINamespace("vr_shell")
public class VrShell extends GvrLayout implements GLSurfaceView.Renderer, OnFrameAvailableListener {
    private Activity mActivity;

    private GLSurfaceView mGlSurfaceView;

    private SurfaceTexture mSurfaceTexture;

    private long mNativeVrShell = 0;

    public VrShell(Activity activity) {
        super(activity);
        mActivity = activity;
    }

    public void onNativeLibraryReady() {
        mNativeVrShell = nativeInit();
        mGlSurfaceView = new GLSurfaceView(getContext());
        mGlSurfaceView.setEGLContextClientVersion(2);
        mGlSurfaceView.setEGLConfigChooser(8, 8, 8, 0, 0, 0);
        mGlSurfaceView.setPreserveEGLContextOnPause(true);
        mGlSurfaceView.setRenderer(this);
        setPresentationView(mGlSurfaceView);

        if (setAsyncReprojectionEnabled(true)) {
            AndroidCompat.setSustainedPerformanceMode(mActivity, true);
        }
    }

    @Override
    public void onFrameAvailable(SurfaceTexture surfaceTexture) {
        mGlSurfaceView.queueEvent(new Runnable() {
            @Override
            public void run() {
                try {
                    mSurfaceTexture.updateTexImage();
                } catch (IllegalStateException e) {
                }
            }
        });
    }

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        // This handle doesn't get deleted anywhere because we create it in onSurfaceCreated() and
        // rely on onSurfaceDestroyed() according to the GLSurfaceView documentation to delete the
        // context and clean up resources.
        int textureDataHandle = createExternalTextureHandle();
        mSurfaceTexture = new SurfaceTexture(textureDataHandle);
        mSurfaceTexture.setOnFrameAvailableListener(this);
        // TODO(bshe): Use this SurfaceTexture to create a Surface and then pass the Surface to a
        // compositor to get frames of web page.
        nativeGvrInit(mNativeVrShell, getGvrApi().getNativeGvrContext());
        nativeInitializeGl(mNativeVrShell, textureDataHandle);
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {}

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return false;
    }

    @Override
    public void onDrawFrame(GL10 gl) {
        nativeDrawFrame(mNativeVrShell);
    }

    @Override
    public void onResume() {
        super.onResume();
        if (mNativeVrShell != 0) {
            nativeOnResume(mNativeVrShell);
        }
        AndroidCompat.setVrModeEnabled(mActivity, true);
    }

    @Override
    public void onPause() {
        super.onPause();
        if (mNativeVrShell != 0) {
            nativeOnPause(mNativeVrShell);
        }
    }

    @Override
    public void shutdown() {
        super.shutdown();
        if (mNativeVrShell != 0) {
            nativeDestroy(mNativeVrShell);
        }
        if (mSurfaceTexture != null) {
            mSurfaceTexture.release();
        }
    }

    /**
     * Create a new GLES11Ext.GL_TEXTURE_EXTERNAL_OES texture handle.
     * @return New texture handle.
     */
    private int createExternalTextureHandle() {
        int[] textureDataHandle = new int[1];
        glGenTextures(1, textureDataHandle, 0);
        if (textureDataHandle[0] != 0) {
            glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, textureDataHandle[0]);
            glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            return textureDataHandle[0];
        } else {
            throw new RuntimeException("Error generating texture handle.");
        }
    }

    private native long nativeInit();

    private native void nativeGvrInit(long nativeVrShell, long nativeGvrApi);

    private native void nativeDestroy(long nativeVrShell);

    private native void nativeInitializeGl(long nativeVrShell, int textureDataHandle);

    private native void nativeDrawFrame(long nativeVrShell);

    private native void nativeOnPause(long nativeVrShell);

    private native void nativeOnResume(long nativeVrShell);
}

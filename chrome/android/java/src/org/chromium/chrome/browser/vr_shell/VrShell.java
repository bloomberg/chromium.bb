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
import android.content.Context;
import android.graphics.SurfaceTexture;
import android.graphics.SurfaceTexture.OnFrameAvailableListener;
import android.opengl.GLES11Ext;
import android.opengl.GLSurfaceView;
import android.os.StrictMode;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import com.google.vr.ndk.base.AndroidCompat;
import com.google.vr.ndk.base.GvrLayout;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * This view extends from GvrLayout which wraps a GLSurfaceView that renders VR shell.
 */
@JNINamespace("vr_shell")
public class VrShell extends GvrLayout implements GLSurfaceView.Renderer, VrShellInterface {
    private static final String TAG = "VrShell";

    public static final String VR_EXTRA = com.google.vr.sdk.base.Constants.EXTRA_VR_LAUNCH;

    private Activity mActivity;

    private final GLSurfaceView mGlSurfaceView;

    private long mNativeVrShell = 0;

    private int mContentTextureHandle;
    private FrameListener mContentFrameListener;

    private ContentViewCoreContainer mContentViewCoreContainer;

    // The tab that holds the main ContentViewCore.
    private Tab mTab;

    // The ContentViewCore for the main content rect in VR.
    private ContentViewCore mCVC;

    // The non-VR container view for mCVC.
    private ViewGroup mOriginalContentViewParent;

    // TODO(mthiesse): Instead of caching these values, make tab reparenting work for this case.
    private int mOriginalContentViewIndex;
    private ViewGroup.LayoutParams mOriginalLayoutParams;
    private WindowAndroid mOriginalWindowAndroid;

    private VrWindowAndroid mVRWindowAndroid;

    private static class ContentViewCoreContainer extends FrameLayout {
        public ContentViewCoreContainer(Context context) {
            super(context);
        }

        @Override
        public boolean dispatchTouchEvent(MotionEvent event) {
            return true;
        }
    }

    public VrShell(Activity activity) {
        super(activity);
        mActivity = activity;
        mContentViewCoreContainer = new ContentViewCoreContainer(activity);
        addView(mContentViewCoreContainer, 0, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));
        mGlSurfaceView = new GLSurfaceView(getContext());
        mGlSurfaceView.setEGLContextClientVersion(2);
        mGlSurfaceView.setEGLConfigChooser(8, 8, 8, 8, 0, 0);
        mGlSurfaceView.setPreserveEGLContextOnPause(true);
        mGlSurfaceView.setRenderer(this);
        setPresentationView(mGlSurfaceView);

        if (setAsyncReprojectionEnabled(true)) {
            AndroidCompat.setSustainedPerformanceMode(mActivity, true);
        }
    }

    @Override
    public void onNativeLibraryReady(Tab currentTab) {
        assert currentTab.getContentViewCore() != null;
        mTab = currentTab;
        mCVC = mTab.getContentViewCore();
        mVRWindowAndroid = new VrWindowAndroid(mActivity);

        mNativeVrShell = nativeInit(mCVC.getWebContents(),
                mVRWindowAndroid.getNativePointer());

        reparentContentWindow();

        nativeUpdateCompositorLayers(mNativeVrShell);
    }

    private void reparentContentWindow() {
        mOriginalWindowAndroid = mCVC.getWindowAndroid();

        // TODO(mthiesse): Update the WindowAndroid in ChromeActivity too?
        mTab.updateWindowAndroid(null);
        mTab.updateWindowAndroid(mVRWindowAndroid);

        ViewGroup contentContentView = mCVC.getContainerView();
        mOriginalContentViewParent = ((ViewGroup) contentContentView.getParent());
        mOriginalContentViewIndex = mOriginalContentViewParent.indexOfChild(contentContentView);
        mOriginalLayoutParams = contentContentView.getLayoutParams();
        mOriginalContentViewParent.removeView(contentContentView);

        mContentViewCoreContainer.addView(contentContentView, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));
    }

    private void restoreContentWindow() {
        ViewGroup contentContentView = mCVC.getContainerView();
        mTab.updateWindowAndroid(null);
        mTab.updateWindowAndroid(mOriginalWindowAndroid);
        mContentViewCoreContainer.removeView(contentContentView);
        mOriginalContentViewParent.addView(contentContentView, mOriginalContentViewIndex,
                mOriginalLayoutParams);
    }

    private static class FrameListener implements OnFrameAvailableListener {
        final SurfaceTexture mSurfaceTexture;
        final GLSurfaceView mGlSurfaceView;
        boolean mFirstTex = true;

        final Runnable mUpdateTexImage = new Runnable() {
            @Override
            public void run() {
                try {
                    mSurfaceTexture.updateTexImage();
                } catch (IllegalStateException e) {
                }
            }
        };

        public FrameListener(SurfaceTexture texture, GLSurfaceView glSurfaceView) {
            mSurfaceTexture = texture;
            mSurfaceTexture.setOnFrameAvailableListener(this);
            mGlSurfaceView = glSurfaceView;
        }

        @Override
        public void onFrameAvailable(SurfaceTexture surfaceTexture) {
            mFirstTex = false;
            mGlSurfaceView.queueEvent(mUpdateTexImage);
        }
    }

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        final int width = mCVC.getContainerView().getWidth();
        final int height = mCVC.getContainerView().getHeight();
        mContentTextureHandle = createExternalTextureHandle();
        mContentFrameListener = new FrameListener(new SurfaceTexture(mContentTextureHandle),
                mGlSurfaceView);
        mContentFrameListener.mSurfaceTexture.setDefaultBufferSize(width, height);

        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                nativeContentSurfaceChanged(mNativeVrShell, width, height,
                        new Surface(mContentFrameListener.mSurfaceTexture));
                mCVC.onPhysicalBackingSizeChanged(width, height);
            }
        });

        nativeGvrInit(mNativeVrShell, getGvrApi().getNativeGvrContext());
        nativeInitializeGl(mNativeVrShell, mContentTextureHandle);
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {}

    @Override
    public void onDrawFrame(GL10 gl) {
        // Make sure we've updated the texture at least once. We do this because onFrameAvailable
        // isn't guaranteed to have fired after transitioning to VR. It only fires when the texture
        // is updated either through scrolling, resizing, etc. - none of which we're guaranteed to
        // have done on transition.
        if (mContentFrameListener.mFirstTex) {
            mContentFrameListener.mSurfaceTexture.updateTexImage();
            mContentFrameListener.mFirstTex = false;
        }
        nativeDrawFrame(mNativeVrShell);
    }

    @Override
    public void onResume() {
        super.onResume();
        if (mNativeVrShell != 0) {
            // Refreshing the viewer profile accesses disk, so we need to temporarily allow disk
            // reads. The GVR team promises this will be fixed when they launch.
            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
            try {
                nativeOnResume(mNativeVrShell);
            } finally {
                StrictMode.setThreadPolicy(oldPolicy);
            }
        }
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
        if (mContentFrameListener != null && mContentFrameListener.mSurfaceTexture != null) {
            mContentFrameListener.mSurfaceTexture.release();
        }
        restoreContentWindow();
    }

    @Override
    public void pause() {
        onPause();
    }

    @Override
    public void resume() {
        onResume();
    }

    @Override
    public void teardown() {
        shutdown();
    }

    @Override
    public void setVrModeEnabled(boolean enabled) {
        AndroidCompat.setVrModeEnabled(mActivity, enabled);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return true;
    }

    @Override
    public FrameLayout getContainer() {
        return (FrameLayout) this;
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

    private native long nativeInit(WebContents contentWebContents,
            long nativeContentWindowAndroid);

    private native void nativeGvrInit(long nativeVrShell, long nativeGvrApi);

    private native void nativeDestroy(long nativeVrShell);

    private native void nativeInitializeGl(
            long nativeVrShell, int contentDataHandle);

    private native void nativeDrawFrame(long nativeVrShell);

    private native void nativeOnPause(long nativeVrShell);

    private native void nativeOnResume(long nativeVrShell);

    private native void nativeContentSurfaceDestroyed(long nativeVrShell);

    private native void nativeContentSurfaceChanged(
            long nativeVrShell, int width, int height, Surface surface);

    private native void nativeUpdateCompositorLayers(long nativeVrShell);
}

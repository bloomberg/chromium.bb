// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.graphics.Point;
import android.os.StrictMode;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.FrameLayout;

import com.google.vr.ndk.base.AndroidCompat;
import com.google.vr.ndk.base.GvrLayout;

import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabRedirectHandler;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.VirtualDisplayAndroid;

/**
 * This view extends from GvrLayout which wraps a GLSurfaceView that renders VR shell.
 */
@JNINamespace("vr_shell")
public class VrShellImpl extends GvrLayout implements VrShell, SurfaceHolder.Callback {
    private static final String TAG = "VrShellImpl";

    // TODO(mthiesse): These values work well for Pixel/Pixel XL in VR, but we need to come up with
    // a way to compute good values for any screen size/scaling ratio.

    // Increasing DPR any more than this doesn't appear to increase text quality.
    private static final float DEFAULT_DPR = 1.2f;
    // For WebVR we just create a DPR 1.0 display that matches the physical display size.
    private static final float WEBVR_DPR = 1.0f;
    // Fairly arbitrary values that put a good amount of content on the screen without making the
    // text too small to read.
    private static final float DEFAULT_CONTENT_WIDTH = 1024f;
    private static final float DEFAULT_CONTENT_HEIGHT = 576f;
    // Temporary values that will be changed when the UI loads and figures out how what size it
    // needs to be.
    private static final float DEFAULT_UI_WIDTH = 1920f;
    private static final float DEFAULT_UI_HEIGHT = 1080f;

    private final Activity mActivity;
    private final CompositorViewHolder mCompositorViewHolder;
    private final VirtualDisplayAndroid mContentVirtualDisplay;
    private final VirtualDisplayAndroid mUiVirtualDisplay;

    private long mNativeVrShell;

    private FrameLayout mUiCVCContainer;
    private View mPresentationView;

    // The tab that holds the main ContentViewCore.
    private Tab mTab;

    // The ContentViewCore for the main content rect in VR.
    private ContentViewCore mContentCVC;
    private WindowAndroid mOriginalWindowAndroid;
    private VrWindowAndroid mContentVrWindowAndroid;

    private WebContents mUiContents;
    private ContentViewCore mUiCVC;
    private VrWindowAndroid mUiVrWindowAndroid;

    private boolean mReprojectedRendering;

    private TabRedirectHandler mNonVrTabRedirectHandler;
    private TabModelSelector mTabModelSelector;

    public VrShellImpl(Activity activity, CompositorViewHolder compositorViewHolder) {
        super(activity);
        mActivity = activity;
        mCompositorViewHolder = compositorViewHolder;
        mTabModelSelector = mCompositorViewHolder.detachForVR();
        mUiCVCContainer = new FrameLayout(getContext()) {
            @Override
            public boolean dispatchTouchEvent(MotionEvent event) {
                return true;
            }
        };
        addView(mUiCVCContainer, 0, new FrameLayout.LayoutParams(0, 0));

        mReprojectedRendering = setAsyncReprojectionEnabled(true);
        if (mReprojectedRendering) {
            // No need render to a Surface if we're reprojected. We'll be rendering with surfaceless
            // EGL.
            mPresentationView = new FrameLayout(mActivity);

            // Only enable sustained performance mode when Async reprojection decouples the app
            // framerate from the display framerate.
            AndroidCompat.setSustainedPerformanceMode(mActivity, true);
        } else {
            SurfaceView surfaceView = new SurfaceView(mActivity);
            surfaceView.getHolder().addCallback(this);
            mPresentationView = surfaceView;
        }

        setPresentationView(mPresentationView);

        DisplayAndroid primaryDisplay = DisplayAndroid.getNonMultiDisplay(activity);
        mContentVirtualDisplay = VirtualDisplayAndroid.createVirtualDisplay();
        mContentVirtualDisplay.setTo(primaryDisplay);
        mUiVirtualDisplay = VirtualDisplayAndroid.createVirtualDisplay();
        mUiVirtualDisplay.setTo(primaryDisplay);
    }

    @Override
    public void initializeNative(Tab currentTab, VrShellDelegate delegate, boolean forWebVR) {
        assert currentTab.getContentViewCore() != null;
        mTab = currentTab;
        mContentCVC = mTab.getContentViewCore();
        mContentVrWindowAndroid = new VrWindowAndroid(mActivity, mContentVirtualDisplay);

        // Make sure we are not redirecting to another app, i.e. out of VR mode.
        mNonVrTabRedirectHandler = mTab.getTabRedirectHandler();
        mTab.setTabRedirectHandler(new TabRedirectHandler(mActivity) {
            @Override
            public boolean shouldStayInChrome(boolean hasExternalProtocol) {
                return true;
            }
        });

        mUiVrWindowAndroid = new VrWindowAndroid(mActivity, mUiVirtualDisplay);
        mUiContents = WebContentsFactory.createWebContents(true, false);
        mUiCVC = new ContentViewCore(mActivity, ChromeVersionInfo.getProductVersion());
        ContentView uiContentView = ContentView.createContentView(mActivity, mUiCVC);
        mUiCVC.initialize(ViewAndroidDelegate.createBasicDelegate(uiContentView),
                uiContentView, mUiContents, mUiVrWindowAndroid);

        mNativeVrShell = nativeInit(mContentCVC.getWebContents(),
                mContentVrWindowAndroid.getNativePointer(), mUiContents,
                mUiVrWindowAndroid.getNativePointer(), forWebVR, delegate,
                getGvrApi().getNativeGvrContext(), mReprojectedRendering);

        // Set the UI and content sizes before we load the UI.
        setUiCssSize(DEFAULT_UI_WIDTH, DEFAULT_UI_HEIGHT, DEFAULT_DPR);
        if (forWebVR) {
            DisplayAndroid primaryDisplay = DisplayAndroid.getNonMultiDisplay(mActivity);
            setContentCssSize(primaryDisplay.getPhysicalDisplayWidth(),
                    primaryDisplay.getPhysicalDisplayHeight(), WEBVR_DPR);
        } else {
            setContentCssSize(DEFAULT_CONTENT_WIDTH, DEFAULT_CONTENT_HEIGHT, DEFAULT_DPR);
        }

        nativeLoadUIContent(mNativeVrShell);

        mPresentationView.setOnTouchListener(new View.OnTouchListener() {
            @Override
            @SuppressLint("ClickableViewAccessibility")
            public boolean onTouch(View v, MotionEvent event) {
                if (!CommandLine.getInstance().hasSwitch(ChromeSwitches.ENABLE_VR_SHELL_DEV)
                        && event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                    nativeOnTriggerEvent(mNativeVrShell);
                    return true;
                }
                return false;
            }
        });

        uiContentView.setVisibility(View.VISIBLE);
        mUiCVC.onShow();
        mUiCVCContainer.addView(uiContentView, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));
        mUiCVC.setBottomControlsHeight(0);
        mUiCVC.setTopControlsHeight(0, false);
        mUiVrWindowAndroid.onVisibilityChanged(true);

        mOriginalWindowAndroid = mContentCVC.getWindowAndroid();
        mTab.updateWindowAndroid(mContentVrWindowAndroid);
        mContentCVC.onAttachedToWindow();
    }

    @CalledByNative
    public void setUiCssSize(float width, float height, float dpr) {
        ThreadUtils.assertOnUiThread();
        if (dpr != DEFAULT_DPR) {
            Log.w(TAG, "Changing UI DPR causes the UI to flicker and should generally not be "
                    + "done.");
        }
        int surfaceWidth = (int) Math.ceil(width * dpr);
        int surfaceHeight = (int) Math.ceil(height * dpr);

        Point size = new Point(surfaceWidth, surfaceHeight);
        mUiVirtualDisplay.update(size, size, dpr, null, null, null);
        mUiCVC.onSizeChanged(surfaceWidth, surfaceHeight, 0, 0);
        mUiCVC.onPhysicalBackingSizeChanged(surfaceWidth, surfaceHeight);
        nativeUIPhysicalBoundsChanged(mNativeVrShell, surfaceWidth, surfaceHeight, dpr);
    }

    @CalledByNative
    public void setContentCssSize(float width, float height, float dpr) {
        ThreadUtils.assertOnUiThread();
        int surfaceWidth = (int) Math.ceil(width * dpr);
        int surfaceHeight = (int) Math.ceil(height * dpr);

        Point size = new Point(surfaceWidth, surfaceHeight);
        mContentVirtualDisplay.update(size, size, dpr, null, null, null);
        mContentCVC.onSizeChanged(surfaceWidth, surfaceHeight, 0, 0);
        mContentCVC.onPhysicalBackingSizeChanged(surfaceWidth, surfaceHeight);
        nativeContentPhysicalBoundsChanged(mNativeVrShell, surfaceWidth, surfaceHeight, dpr);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        // Normally, touch event is dispatched to presentation view only if the phone is paired with
        // a Cardboard viewer. This is annoying when we just want to quickly verify a Cardboard
        // behavior. This allows us to trigger cardboard trigger event without pair to a Cardboard.
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.ENABLE_VR_SHELL_DEV)
                && event.getActionMasked() == MotionEvent.ACTION_DOWN) {
            nativeOnTriggerEvent(mNativeVrShell);
        }
        return super.dispatchTouchEvent(event);
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
    public void onLoadProgressChanged(double progress) {
        if (mNativeVrShell != 0) {
            nativeOnLoadProgressChanged(mNativeVrShell, progress);
        }
    }

    @Override
    public void shutdown() {
        if (mNativeVrShell != 0) {
            nativeDestroy(mNativeVrShell);
            mNativeVrShell = 0;
        }
        mCompositorViewHolder.onExitVR(mTabModelSelector);
        mTab.setTabRedirectHandler(mNonVrTabRedirectHandler);
        mTab.updateWindowAndroid(mOriginalWindowAndroid);
        mContentCVC.onSizeChanged(mContentCVC.getContainerView().getWidth(),
                mContentCVC.getContainerView().getHeight(), 0, 0);
        mUiContents.destroy();
        mContentVirtualDisplay.destroy();
        mUiVirtualDisplay.destroy();
        super.shutdown();
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
    public void setWebVrModeEnabled(boolean enabled) {
        nativeSetWebVrMode(mNativeVrShell, enabled);
    }

    @Override
    public FrameLayout getContainer() {
        return this;
    }

    @Override
    public void setCloseButtonListener(Runnable runner) {
        getUiLayout().setCloseButtonListener(runner);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        nativeSetSurface(mNativeVrShell, holder.getSurface());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        // No need to do anything here, we don't care about surface width/height.
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        // TODO(mthiesse): For now we don't need to handle this because we exit VR on activity pause
        // (which destroys the surface). If in the future we don't destroy VR Shell on exiting,
        // we will need to handle this, or at least properly handle surfaceCreated being called
        // multiple times.
    }

    private native long nativeInit(WebContents contentWebContents,
            long nativeContentWindowAndroid, WebContents uiWebContents, long nativeUiWindowAndroid,
            boolean forWebVR, VrShellDelegate delegate, long gvrApi, boolean reprojectedRendering);
    private native void nativeSetSurface(long nativeVrShell, Surface surface);
    private native void nativeLoadUIContent(long nativeVrShell);
    private native void nativeDestroy(long nativeVrShell);
    private native void nativeOnTriggerEvent(long nativeVrShell);
    private native void nativeOnPause(long nativeVrShell);
    private native void nativeOnResume(long nativeVrShell);
    private native void nativeOnLoadProgressChanged(long nativeVrShell, double progress);
    private native void nativeContentPhysicalBoundsChanged(long nativeVrShell, int width,
            int height, float dpr);
    private native void nativeUIPhysicalBoundsChanged(long nativeVrShell, int width, int height,
            float dpr);
    private native void nativeSetWebVrMode(long nativeVrShell, boolean enabled);
}

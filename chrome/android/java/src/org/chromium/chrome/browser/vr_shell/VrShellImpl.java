// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.graphics.Point;
import android.os.StrictMode;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
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
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabContentViewParent;
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
public class VrShellImpl extends GvrLayout implements VrShell {
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
    private final VirtualDisplayAndroid mContentVirtualDisplay;
    private final VirtualDisplayAndroid mUiVirtualDisplay;

    private long mNativeVrShell;

    private FrameLayout mContentCVCContainer;
    private FrameLayout mUiCVCContainer;
    private FrameLayout mPresentationView;

    // The tab that holds the main ContentViewCore.
    private Tab mTab;

    // The ContentViewCore for the main content rect in VR.
    private ContentViewCore mContentCVC;
    private TabContentViewParent mTabParent;
    private ViewGroup mTabParentParent;

    // TODO(mthiesse): Instead of caching these values, make tab reparenting work for this case.
    private int mOriginalTabParentIndex;
    private ViewGroup.LayoutParams mOriginalLayoutParams;
    private WindowAndroid mOriginalWindowAndroid;

    private VrWindowAndroid mContentVrWindowAndroid;

    private WebContents mUiContents;
    private ContentViewCore mUiCVC;
    private VrWindowAndroid mUiVrWindowAndroid;

    public VrShellImpl(Activity activity) {
        super(activity);
        mActivity = activity;
        mContentCVCContainer = new FrameLayout(getContext()) {
            @Override
            public boolean dispatchTouchEvent(MotionEvent event) {
                return true;
            }
        };
        mUiCVCContainer = new FrameLayout(getContext()) {
            @Override
            public boolean dispatchTouchEvent(MotionEvent event) {
                return true;
            }
        };
        addView(mContentCVCContainer, 0, new FrameLayout.LayoutParams(0, 0));
        addView(mUiCVCContainer, 0, new FrameLayout.LayoutParams(0, 0));

        mPresentationView = new FrameLayout(mActivity);
        setPresentationView(mPresentationView);

        if (setAsyncReprojectionEnabled(true)) {
            AndroidCompat.setSustainedPerformanceMode(mActivity, true);
        }
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

        mUiVrWindowAndroid = new VrWindowAndroid(mActivity, mUiVirtualDisplay);
        mUiContents = WebContentsFactory.createWebContents(true, false);
        mUiCVC = new ContentViewCore(mActivity, ChromeVersionInfo.getProductVersion());
        ContentView uiContentView = ContentView.createContentView(mActivity, mUiCVC);
        mUiCVC.initialize(ViewAndroidDelegate.createBasicDelegate(uiContentView),
                uiContentView, mUiContents, mUiVrWindowAndroid);

        mNativeVrShell = nativeInit(mContentCVC.getWebContents(),
                mContentVrWindowAndroid.getNativePointer(), mUiContents,
                mUiVrWindowAndroid.getNativePointer(), forWebVR, delegate,
                getGvrApi().getNativeGvrContext());

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

        reparentContentWindow();
    }

    private void reparentContentWindow() {
        mOriginalWindowAndroid = mContentCVC.getWindowAndroid();

        mTab.updateWindowAndroid(mContentVrWindowAndroid);

        mTabParent = mTab.getView();
        mTabParentParent = (ViewGroup) mTabParent.getParent();
        mOriginalTabParentIndex = mTabParentParent.indexOfChild(mTabParent);
        mOriginalLayoutParams = mTabParent.getLayoutParams();
        mTabParentParent.removeView(mTabParent);

        mContentCVCContainer.addView(mTabParent, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
    }

    private void restoreContentWindow() {
        mTab.updateWindowAndroid(mOriginalWindowAndroid);

        // If the tab's view has changed, the necessary view reparenting has already been done.
        if (mTab.getView() == mTabParent) {
            mContentCVCContainer.removeView(mTabParent);
            mTabParentParent.addView(mTabParent, mOriginalTabParentIndex, mOriginalLayoutParams);
            mTabParent.requestFocus();
        }
        mTabParent = null;
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

        mUiCVCContainer.setLayoutParams(new FrameLayout.LayoutParams(surfaceWidth, surfaceHeight));
        mUiCVC.onPhysicalBackingSizeChanged(surfaceWidth, surfaceHeight);
        nativeUIBoundsChanged(mNativeVrShell, surfaceWidth, surfaceHeight, dpr);
    }

    @CalledByNative
    public void setContentCssSize(float width, float height, float dpr) {
        ThreadUtils.assertOnUiThread();
        int surfaceWidth = (int) Math.ceil(width * dpr);
        int surfaceHeight = (int) Math.ceil(height * dpr);

        Point size = new Point(surfaceWidth, surfaceHeight);
        mContentVirtualDisplay.update(size, size, dpr, null, null, null);

        mContentCVCContainer.setLayoutParams(new FrameLayout.LayoutParams(
                surfaceWidth,  surfaceHeight));
        mContentCVC.onPhysicalBackingSizeChanged(surfaceWidth, surfaceHeight);
        nativeContentBoundsChanged(mNativeVrShell, surfaceWidth, surfaceHeight, dpr);
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
    public void shutdown() {
        if (mNativeVrShell != 0) {
            nativeDestroy(mNativeVrShell);
            mNativeVrShell = 0;
        }
        restoreContentWindow();
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
    public void setVrModeEnabled(boolean enabled) {
        AndroidCompat.setVrModeEnabled(mActivity, enabled);
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

    private native long nativeInit(WebContents contentWebContents,
            long nativeContentWindowAndroid, WebContents uiWebContents, long nativeUiWindowAndroid,
            boolean forWebVR, VrShellDelegate delegate, long gvrApi);
    private native void nativeLoadUIContent(long nativeVrShell);
    private native void nativeDestroy(long nativeVrShell);
    private native void nativeOnTriggerEvent(long nativeVrShell);
    private native void nativeOnPause(long nativeVrShell);
    private native void nativeOnResume(long nativeVrShell);
    private native void nativeContentBoundsChanged(long nativeVrShell, int width, int height,
            float dpr);
    private native void nativeUIBoundsChanged(long nativeVrShell, int width, int height, float dpr);
    private native void nativeSetWebVrMode(long nativeVrShell, boolean enabled);
}

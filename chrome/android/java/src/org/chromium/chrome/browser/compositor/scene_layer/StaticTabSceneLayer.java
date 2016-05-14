// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.scene_layer;

import android.content.Context;
import android.graphics.Rect;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.Layout.SizingFlags;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.widget.ClipDrawableProgressBar.DrawingInfo;
import org.chromium.chrome.browser.widget.ControlContainer;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.resources.ResourceManager;

/**
 * A SceneLayer to render a static tab.
 */
@JNINamespace("chrome::android")
public class StaticTabSceneLayer extends SceneLayer {
    // NOTE: If you use SceneLayer's native pointer here, the JNI generator will try to
    // downcast using reinterpret_cast<>. We keep a separate pointer to avoid it.
    private long mNativePtr;

    private final Context mContext;

    private final int mResToolbarControlContainer;

    private DrawingInfo mProgressBarDrawingInfo;
    private final boolean mIsTablet;

    public StaticTabSceneLayer(Context context, int resToolbarControlContainer) {
        mContext = context;
        mResToolbarControlContainer = resToolbarControlContainer;
        mIsTablet = DeviceFormFactor.isTablet(mContext);
    }

    /**
     * Update {@link StaticTabSceneLayer} with the given parameters.
     *
     * @param dpToPx            The ratio of dp to px.
     * @param contentViewport   The viewport of the content.
     * @param layerTitleCache   The LayerTitleCache.
     * @param tabContentManager The TabContentManager.
     * @param fullscreenManager The FullscreenManager.
     * @param layoutTab         The LayoutTab.
     */
    public void update(float dpToPx, Rect contentViewport, LayerTitleCache layerTitleCache,
            TabContentManager tabContentManager, ChromeFullscreenManager fullscreenManager,
            LayoutTab layoutTab) {
        if (layoutTab == null) {
            return;
        }

        float contentOffset =
                fullscreenManager != null ? fullscreenManager.getContentOffset() : 0.f;

        // TODO(dtrainor, clholgat): remove "* dpToPx" once the native part is fully supporting dp.
        nativeUpdateTabLayer(mNativePtr, contentViewport.left, contentViewport.top,
                contentViewport.width(), contentViewport.height(), tabContentManager,
                layoutTab.getId(), mResToolbarControlContainer, layoutTab.canUseLiveTexture(),
                layoutTab.getBackgroundColor(), layoutTab.getRenderX() * dpToPx,
                layoutTab.getRenderY() * dpToPx, layoutTab.getScaledContentWidth() * dpToPx,
                layoutTab.getScaledContentHeight() * dpToPx, contentOffset,
                layoutTab.getStaticToViewBlend(), layoutTab.getSaturation(),
                layoutTab.getBrightness());
    }

    /**
     * Update the toolbar and progress bar layers.
     *
     * @param dpToPx The conversion factor of dp to px.
     * @param topControlsBackgroundColor The background color of the top controls.
     * @param topControlsUrlBarAlpha The alpha of the URL bar.
     * @param fullscreenManager A ChromeFullscreenManager instance.
     * @param resourceManager A ResourceManager for loading static resources.
     * @param forceHideAndroidTopControls True if the Android top controls are being hidden.
     * @param sizingFlags The sizing flags for the toolbar.
     */
    public void updateToolbarLayer(float dpToPx, int topControlsBackgroundColor,
            float topControlsUrlBarAlpha, ChromeFullscreenManager fullscreenManager,
            ResourceManager resourceManager, boolean forceHideAndroidTopControls,
            int sizingFlags) {
        if (!DeviceClassManager.enableFullscreen()) return;

        if (fullscreenManager == null) return;
        ControlContainer toolbarContainer = fullscreenManager.getControlContainer();
        if (!mIsTablet && toolbarContainer != null) {
            if (mProgressBarDrawingInfo == null) mProgressBarDrawingInfo = new DrawingInfo();
            toolbarContainer.getProgressBarDrawingInfo(mProgressBarDrawingInfo);
        } else {
            assert mProgressBarDrawingInfo == null;
        }

        float offset = fullscreenManager.getControlOffset();
        boolean useTexture = fullscreenManager.drawControlsAsTexture() || offset == 0
                || forceHideAndroidTopControls;

        fullscreenManager.setHideTopControlsAndroidView(forceHideAndroidTopControls);

        if ((sizingFlags & SizingFlags.REQUIRE_FULLSCREEN_SIZE) != 0
                && (sizingFlags & SizingFlags.ALLOW_TOOLBAR_HIDE) == 0
                && (sizingFlags & SizingFlags.ALLOW_TOOLBAR_ANIMATE) == 0) {
            useTexture = false;
        }

        nativeUpdateToolbarLayer(mNativePtr, resourceManager, R.id.control_container,
                topControlsBackgroundColor, R.drawable.textbox, topControlsUrlBarAlpha, offset,
                useTexture, forceHideAndroidTopControls);

        if (mProgressBarDrawingInfo == null) return;
        nativeUpdateProgressBar(mNativePtr,
                mProgressBarDrawingInfo.progressBarRect.left,
                mProgressBarDrawingInfo.progressBarRect.top,
                mProgressBarDrawingInfo.progressBarRect.width(),
                mProgressBarDrawingInfo.progressBarRect.height(),
                mProgressBarDrawingInfo.progressBarColor,
                mProgressBarDrawingInfo.progressBarBackgroundRect.left,
                mProgressBarDrawingInfo.progressBarBackgroundRect.top,
                mProgressBarDrawingInfo.progressBarBackgroundRect.width(),
                mProgressBarDrawingInfo.progressBarBackgroundRect.height(),
                mProgressBarDrawingInfo.progressBarBackgroundColor);
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = nativeInit();
        }
        assert mNativePtr != 0;
    }

    @Override
    public void destroy() {
        super.destroy();
        mNativePtr = 0;
    }

    private native long nativeInit();
    private native void nativeUpdateTabLayer(long nativeStaticTabSceneLayer, float contentViewportX,
            float contentViewportY, float contentViewportWidth, float contentViewportHeight,
            TabContentManager tabContentManager, int id, int toolbarResourceId,
            boolean canUseLiveLayer, int backgroundColor, float x, float y, float width,
            float height, float contentOffsetY, float staticToViewBlend, float saturation,
            float brightness);
    private native void nativeUpdateToolbarLayer(long nativeStaticTabSceneLayer,
            ResourceManager resourceManager, int resourceId, int toolbarBackgroundColor,
            int urlBarResourceId, float urlBarAlpha, float topOffset, boolean visible,
            boolean showShadow);
    private native void nativeUpdateProgressBar(
            long nativeStaticTabSceneLayer, int progressBarX, int progressBarY,
            int progressBarWidth, int progressBarHeight, int progressBarColor,
            int progressBarBackgroundX, int progressBarBackgroundY, int progressBarBackgroundWidth,
            int progressBarBackgroundHeight, int progressBarBackgroundColor);
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.scene_layer;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchIconSpriteControl;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPeekPromoControl;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.ui.resources.ResourceManager;

/**
 * A SceneLayer to render layers for ContextualSearchLayout.
 */
@JNINamespace("chrome::android")
public class ContextualSearchSceneLayer extends SceneLayer {

    // NOTE: If you use SceneLayer's native pointer here, the JNI generator will try to
    // downcast using reinterpret_cast<>. We keep a separate pointer to avoid it.
    private long mNativePtr;

    private final float mDpToPx;

    public ContextualSearchSceneLayer(float dpToPx) {
        mDpToPx = dpToPx;
    }

    /**
     * Update the scene layer to draw an OverlayPanel.
     * @param resourceManager Manager to get view and image resources.
     * @param panel The OverlayPanel to render.
     * @param searchContextViewId The ID of the view for Contextual Search's context.
     * @param searchTermViewId The ID of the view containing the Contextual Search search term.
     * @param peekPromoControl The peeking promotion for Contextual Search; optional if
     *                         "panelType" is READER_MODE_PANEL.
     * @param searchContextOpacity The opacity of the text specified by "searchContextViewId".
     * @param searchTermOpacity The opacity of the text specified by "searchTermViewId".
     * @param spriteControl The object controling the "G" animation for Contextual Search; optional
     *                      if "panelType" is READER_MODE_PANEL.
     */
    public void update(ResourceManager resourceManager,
            OverlayPanel panel,
            int searchContextViewId,
            int searchTermViewId,
            ContextualSearchPeekPromoControl peekPromoControl,
            float searchContextOpacity,
            float searchTermOpacity,
            ContextualSearchIconSpriteControl spriteControl) {

        boolean searchPromoVisible = panel.getPromoVisible();
        float searchPromoHeightPx = panel.getPromoHeightPx();
        float searchPromoOpacity = panel.getPromoOpacity();

        int searchPeekPromoTextViewId = peekPromoControl.getViewId();
        boolean searchPeekPromoVisible = peekPromoControl.isVisible();
        float searchPeekPromoHeightPx = peekPromoControl.getHeightPx();
        float searchPeekPromoPaddingPx =  peekPromoControl.getPaddingPx();
        float searchPeekPromoRippleWidthPx = peekPromoControl.getRippleWidthPx();
        float searchPeekPromoRippleOpacity = peekPromoControl.getRippleOpacity();
        float searchPeekPromoTextOpacity = peekPromoControl.getTextOpacity();

        boolean searchProviderIconSpriteVisible = spriteControl.isVisible();
        float searchProviderIconCompletionPercentage = spriteControl.getCompletionPercentage();

        float searchPanelX = panel.getOffsetX();
        float searchPanelY = panel.getOffsetY();
        float searchPanelWidth = panel.getWidth();
        float searchPanelHeight = panel.getHeight();

        float searchBarMarginSide = panel.getBarMarginSide();
        float searchBarHeight = panel.getBarHeight();

        boolean searchBarBorderVisible = panel.isBarBorderVisible();
        float searchBarBorderHeight = panel.getBarBorderHeight();

        boolean searchBarShadowVisible = panel.getBarShadowVisible();
        float searchBarShadowOpacity = panel.getBarShadowOpacity();

        float arrowIconOpacity = panel.getArrowIconOpacity();
        float arrowIconRotation = panel.getArrowIconRotation();

        float closeIconOpacity = panel.getCloseIconOpacity();

        boolean isProgressBarVisible = panel.isProgressBarVisible();

        float progressBarHeight = panel.getProgressBarHeight();
        float progressBarOpacity = panel.getProgressBarOpacity();
        int progressBarCompletion = panel.getProgressBarCompletion();

        nativeUpdateContextualSearchLayer(mNativePtr,
                R.drawable.contextual_search_bar_background,
                searchContextViewId,
                searchTermViewId,
                R.drawable.contextual_search_bar_shadow,
                R.drawable.google_icon_sprite,
                R.raw.google_icon_sprite,
                R.drawable.breadcrumb_arrow,
                ContextualSearchPanel.CLOSE_ICON_DRAWABLE_ID,
                R.drawable.progress_bar_background,
                R.drawable.progress_bar_foreground,
                R.id.contextual_search_opt_out_promo,
                R.drawable.contextual_search_promo_ripple,
                searchPeekPromoTextViewId,
                mDpToPx,
                panel.getContentViewCore(),
                searchPromoVisible,
                searchPromoHeightPx,
                searchPromoOpacity,
                searchPeekPromoVisible,
                searchPeekPromoHeightPx,
                searchPeekPromoPaddingPx,
                searchPeekPromoRippleWidthPx,
                searchPeekPromoRippleOpacity,
                searchPeekPromoTextOpacity,
                searchPanelX * mDpToPx,
                searchPanelY * mDpToPx,
                searchPanelWidth * mDpToPx,
                searchPanelHeight * mDpToPx,
                searchBarMarginSide * mDpToPx,
                searchBarHeight * mDpToPx,
                searchContextOpacity,
                searchTermOpacity,
                searchBarBorderVisible,
                searchBarBorderHeight * mDpToPx,
                searchBarShadowVisible,
                searchBarShadowOpacity,
                searchProviderIconSpriteVisible,
                searchProviderIconCompletionPercentage,
                arrowIconOpacity,
                arrowIconRotation,
                closeIconOpacity,
                isProgressBarVisible,
                progressBarHeight * mDpToPx,
                progressBarOpacity,
                progressBarCompletion,
                resourceManager);
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = nativeInit();
        }
        assert mNativePtr != 0;
    }

    /**
     * Destroys this object and the corresponding native component.
     */
    @Override
    public void destroy() {
        super.destroy();
        mNativePtr = 0;
    }

    private native long nativeInit();
    private native void nativeUpdateContextualSearchLayer(
            long nativeContextualSearchSceneLayer,
            int searchBarBackgroundResourceId,
            int searchContextResourceId,
            int searchTermResourceId,
            int searchBarShadowResourceId,
            int panelIconResourceId,
            int searchProviderIconSpriteMetadataResourceId,
            int arrowUpResourceId,
            int closeIconResourceId,
            int progressBarBackgroundResourceId,
            int progressBarResourceId,
            int searchPromoResourceId,
            int peekPromoRippleResourceId,
            int peekPromoTextResourceId,
            float dpToPx,
            ContentViewCore contentViewCore,
            boolean searchPromoVisible,
            float searchPromoHeight,
            float searchPromoOpacity,
            boolean searchPeekPromoVisible,
            float searchPeekPromoHeight,
            float searchPeekPromoPaddingPx,
            float searchPeekPromoRippleWidth,
            float searchPeekPromoRippleOpacity,
            float searchPeekPromoTextOpacity,
            float searchPanelX,
            float searchPanelY,
            float searchPanelWidth,
            float searchPanelHeight,
            float searchBarMarginSide,
            float searchBarHeight,
            float searchContextOpacity,
            float searchTermOpacity,
            boolean searchBarBorderVisible,
            float searchBarBorderHeight,
            boolean searchBarShadowVisible,
            float searchBarShadowOpacity,
            boolean searchProviderIconSpriteVisible,
            float searchProviderIconCompletionPercentage,
            float arrowIconOpacity,
            float arrowIconRotation,
            float closeIconOpacity,
            boolean isProgressBarVisible,
            float progressBarHeight,
            float progressBarOpacity,
            int progressBarCompletion,
            ResourceManager resourceManager);
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.scene_layer;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchIconSpriteControl;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPeekPromoControl;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.ui.resources.ResourceManager;

import javax.annotation.Nullable;

/**
 * A SceneLayer to render layers for ContextualSearchLayout.
 */
@JNINamespace("chrome::android")
public class ContextualSearchSceneLayer extends SceneLayer {
    // NOTE: If you use SceneLayer's native pointer here, the JNI generator will try to
    // downcast using reinterpret_cast<>. We keep a separate pointer to avoid it.
    private long mNativePtr;

    private final float mDpToPx;
    private final ContextualSearchPanel mSearchPanel;

    public ContextualSearchSceneLayer(float dpToPx, ContextualSearchPanel searchPanel) {
        mDpToPx = dpToPx;
        mSearchPanel = searchPanel;
    }

    /**
     * Update contextual search's layer tree using the parameters.
     *
     * @param contentViewCore The CVC, may be null if only updating the bar.
     * @param resourceManager
     */
    public void update(@Nullable ContentViewCore contentViewCore, ResourceManager resourceManager) {
        int searchContextViewId = mSearchPanel.getSearchContextViewId();
        int searchTermViewId = mSearchPanel.getSearchTermViewId();

        boolean searchPromoVisible = mSearchPanel.getPromoVisible();
        float searchPromoHeightPx = mSearchPanel.getPromoHeightPx();
        float searchPromoOpacity = mSearchPanel.getPromoOpacity();

        ContextualSearchPeekPromoControl peekPromoControl = mSearchPanel.getPeekPromoControl();
        int searchPeekPromoTextViewId = peekPromoControl.getViewId();
        boolean searchPeekPromoVisible = peekPromoControl.isVisible();
        float searchPeekPromoHeightPx = peekPromoControl.getHeightPx();
        float searchPeekPromoPaddingPx = peekPromoControl.getPaddingPx();
        float searchPeekPromoRippleWidthPx = peekPromoControl.getRippleWidthPx();
        float searchPeekPromoRippleOpacity = peekPromoControl.getRippleOpacity();
        float searchPeekPromoTextOpacity = peekPromoControl.getTextOpacity();

        float searchPanelX = mSearchPanel.getOffsetX();
        float searchPanelY = mSearchPanel.getOffsetY();
        float searchPanelWidth = mSearchPanel.getWidth();
        float searchPanelHeight = mSearchPanel.getHeight();

        float searchBarMarginSide = mSearchPanel.getSearchBarMarginSide();
        float searchBarHeight = mSearchPanel.getSearchBarHeight();
        float searchContextOpacity = mSearchPanel.getSearchBarContextOpacity();
        float searchTermOpacity = mSearchPanel.getSearchBarTermOpacity();

        boolean searchBarBorderVisible = mSearchPanel.isSearchBarBorderVisible();
        float searchBarBorderHeight = mSearchPanel.getSearchBarBorderHeight();

        boolean searchBarShadowVisible = mSearchPanel.getSearchBarShadowVisible();
        float searchBarShadowOpacity = mSearchPanel.getSearchBarShadowOpacity();

        ContextualSearchIconSpriteControl spriteControl =
                mSearchPanel.getIconSpriteControl();
        boolean searchProviderIconSpriteVisible = spriteControl.isVisible();
        float searchProviderIconCompletionPercentage = spriteControl.getCompletionPercentage();
        float searchProviderIconSpriteSize = spriteControl.getSizePx();

        float arrowIconOpacity = mSearchPanel.getArrowIconOpacity();
        float arrowIconRotation = mSearchPanel.getArrowIconRotation();

        float closeIconOpacity = mSearchPanel.getCloseIconOpacity();

        boolean isProgressBarVisible = mSearchPanel.isProgressBarVisible();

        float progressBarHeight = mSearchPanel.getProgressBarHeight();
        float progressBarOpacity = mSearchPanel.getProgressBarOpacity();
        int progressBarCompletion = mSearchPanel.getProgressBarCompletion();

        nativeUpdateContextualSearchLayer(mNativePtr,
                R.drawable.contextual_search_bar_background,
                searchContextViewId,
                searchTermViewId,
                R.drawable.contextual_search_bar_shadow,
                0, // Passing 0 so that the icon sprite will be used instead of a static icon.
                R.drawable.breadcrumb_arrow,
                ContextualSearchPanel.CLOSE_ICON_DRAWABLE_ID,
                R.drawable.progress_bar_background,
                R.drawable.progress_bar_foreground,
                R.id.contextual_search_opt_out_promo,
                R.drawable.contextual_search_promo_ripple,
                searchPeekPromoTextViewId,
                R.drawable.google_icon_sprite,
                R.raw.google_icon_sprite,
                contentViewCore,
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
                searchProviderIconSpriteSize,
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
            int arrowUpResourceId,
            int closeIconResourceId,
            int progressBarBackgroundResourceId,
            int progressBarResourceId,
            int searchPromoResourceId,
            int peekPromoRippleResourceId,
            int peekPromoTextResourceId,
            int searchProviderIconSpriteBitmapResourceId,
            int searchProviderIconSpriteMetadataResourceId,
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
            float searchProviderIconSpriteSize,
            float arrowIconOpacity,
            float arrowIconRotation,
            float closeIconOpacity,
            boolean isProgressBarVisible,
            float progressBarHeight,
            float progressBarOpacity,
            int progressBarCompletion,
            ResourceManager resourceManager);
}

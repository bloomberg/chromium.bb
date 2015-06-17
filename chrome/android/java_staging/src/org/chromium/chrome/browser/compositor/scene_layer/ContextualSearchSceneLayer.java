// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.scene_layer;

import org.chromium.base.JNINamespace;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
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
        boolean searchPromoVisible = mSearchPanel.getPromoVisible();
        float searchPromoHeightPx = mSearchPanel.getPromoHeightPx();
        float searchPromoOpacity = mSearchPanel.getPromoOpacity();

        float searchPanelY = mSearchPanel.getContextualSearchPanelY();
        float searchPanelWidth = mSearchPanel.getWidth();
        float searchBarMarginTop = mSearchPanel.getSearchBarMarginTop();
        float searchBarHeight = mSearchPanel.getSearchBarHeight();
        float searchBarTextOpacity = mSearchPanel.getSearchBarTextOpacity();

        boolean searchBarBorderVisible = mSearchPanel.isSearchBarBorderVisible();
        float searchBarBorderY = mSearchPanel.getSearchBarBorderY();
        float searchBarBorderHeight = mSearchPanel.getSearchBarBorderHeight();

        boolean searchBarShadowVisible = mSearchPanel.getSearchBarShadowVisible();
        float searchBarShadowOpacity = mSearchPanel.getSearchBarShadowOpacity();

        float searchProviderIconOpacity = mSearchPanel.getSearchProviderIconOpacity();
        float searchIconPaddingLeft = mSearchPanel.getSearchIconPaddingLeft();
        float searchIconOpacity = mSearchPanel.getSearchIconOpacity();

        boolean isProgressBarVisible = mSearchPanel.isProgressBarVisible();
        float progressBarY = mSearchPanel.getProgressBarY();
        float progressBarHeight = mSearchPanel.getProgressBarHeight();
        float progressBarOpacity = mSearchPanel.getProgressBarOpacity();
        int progressBarCompletion = mSearchPanel.getProgressBarCompletion();

        nativeUpdateContextualSearchLayer(mNativePtr,
                R.drawable.contextual_search_bar_background,
                R.id.contextual_search_view,
                R.drawable.contextual_search_bar_shadow,
                R.drawable.blue_google_icon,
                R.drawable.ic_search,
                R.drawable.progress_bar_background,
                R.drawable.progress_bar_foreground,
                R.id.contextual_search_opt_out_promo,
                contentViewCore,
                searchPromoVisible,
                searchPromoHeightPx,
                searchPromoOpacity,
                searchPanelY * mDpToPx,
                searchPanelWidth * mDpToPx,
                searchBarMarginTop * mDpToPx,
                searchBarHeight * mDpToPx,
                searchBarTextOpacity,
                searchBarBorderVisible,
                searchBarBorderY * mDpToPx,
                searchBarBorderHeight * mDpToPx,
                searchBarShadowVisible,
                searchBarShadowOpacity,
                searchProviderIconOpacity,
                searchIconPaddingLeft * mDpToPx,
                searchIconOpacity,
                isProgressBarVisible,
                progressBarY * mDpToPx,
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
            int searchBarTextResourceId,
            int searchBarShadowResourceId,
            int searchProviderIconResourceId,
            int searchIconResourceId,
            int progressBarBackgroundResourceId,
            int progressBarResourceId,
            int searchPromoResourceId,
            ContentViewCore contentViewCore,
            boolean searchPromoVisible,
            float searchPromoHeight,
            float searchPromoOpacity,
            float searchPanelY,
            float searchPanelWidth,
            float searchBarMarginTop,
            float searchBarHeight,
            float searchBarTextOpacity,
            boolean searchBarBorderVisible,
            float searchBarBorderY,
            float searchBarBorderHeight,
            boolean searchBarShadowVisible,
            float searchBarShadowOpacity,
            float searchProviderIconOpacity,
            float searchIconPaddingLeft,
            float searchIconOpacity,
            boolean isProgressBarVisible,
            float progressBarY,
            float progressBarHeight,
            float progressBarOpacity,
            int progressBarCompletion,
            ResourceManager resourceManager);
}

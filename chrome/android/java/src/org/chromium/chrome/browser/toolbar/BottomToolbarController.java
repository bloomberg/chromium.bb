// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnTouchListener;
import android.view.ViewGroup;
import android.view.ViewStub;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.scene_layer.ScrollingBottomViewSceneLayer;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.BottomToolbarModel.PropertyKey;
import org.chromium.chrome.browser.toolbar.BottomToolbarViewBinder.ViewHolder;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

/**
 * The controller for the bottom toolbar. This class handles all interactions that the bottom
 * toolbar has with the outside world. This class has two primary components, an Android view that
 * handles user actions and a composited texture that draws when the controls are being scrolled
 * off-screen. The Android version does not draw unless the controls offset is 0.
 */
public class BottomToolbarController {
    /** The mediator that handles events from outside the bottom toolbar. */
    private final BottomToolbarMediator mMediator;

    /** The tab switcher button component that lives in the bottom toolbar. */
    private final TabSwitcherButtonCoordinator mTabSwitcherButtonCoordinator;

    /** The resource ID used to reference the view bitmap in native. */
    private final int mResourceId;

    /** The resource adapter for the composited bottom toolbar view. */
    private final ViewResourceAdapter mViewResourceAdapter;

    /**
     * Build the controller that manages the bottom toolbar.
     * @param fullscreenManager A {@link ChromeFullscreenManager} to update the bottom controls
     *                          height for the renderer.
     * @param root The root {@link ViewGroup} for locating the vies to inflate.
     */
    public BottomToolbarController(ChromeFullscreenManager fullscreenManager, ViewGroup root) {
        BottomToolbarModel model = new BottomToolbarModel();
        mMediator = new BottomToolbarMediator(model, fullscreenManager, root.getResources());

        int shadowHeight =
                root.getResources().getDimensionPixelOffset(R.dimen.toolbar_shadow_height);

        // This is the Android view component of the views that constitute the bottom toolbar.
        View inflatedView = ((ViewStub) root.findViewById(R.id.bottom_toolbar)).inflate();
        final ScrollingBottomViewResourceFrameLayout toolbarRoot =
                (ScrollingBottomViewResourceFrameLayout) inflatedView;
        toolbarRoot.setTopShadowHeight(shadowHeight);

        mResourceId = toolbarRoot.getId();
        mViewResourceAdapter = toolbarRoot.getResourceAdapter();

        // This is the compositor component of the bottom toolbar views.
        final ScrollingBottomViewSceneLayer sceneLayer =
                new ScrollingBottomViewSceneLayer(toolbarRoot, shadowHeight);

        PropertyModelChangeProcessor<BottomToolbarModel, ViewHolder, PropertyKey> processor =
                new PropertyModelChangeProcessor<>(model, new ViewHolder(sceneLayer, toolbarRoot),
                        new BottomToolbarViewBinder());
        model.addObserver(processor);
        mTabSwitcherButtonCoordinator = new TabSwitcherButtonCoordinator(toolbarRoot);
    }

    /**
     * Initialize the bottom toolbar with the components that had native initialization
     * dependencies.
     * <p>
     * Calling this must occur after the native library have completely loaded.
     * @param resourceManager A {@link ResourceManager} for loading textures into the compositor.
     * @param layoutManager A {@link LayoutManager} to attach overlays to.
     * @param tabSwitcherListener An {@link OnClickListener} that is triggered when the
     *                                  tab switcher button is clicked.
     * @param searchAcceleratorListener An {@link OnClickListener} that is triggered when the
     *                                  search accelerator is clicked.
     * @param menuButtonListener An {@link OnTouchListener} that is triggered when the
     *                           menu button is clicked.
     * @param tabModelSelector A {@link TabModelSelector} that the tab switcher button uses to
     *                         keep its tab count updated.
     */
    public void initializeWithNative(ResourceManager resourceManager, LayoutManager layoutManager,
            OnClickListener tabSwitcherListener, OnClickListener searchAcceleratorListener,
            OnTouchListener menuButtonListener, TabModelSelector tabModelSelector) {
        mMediator.setButtonListeners(searchAcceleratorListener, menuButtonListener);
        mMediator.setLayoutManager(layoutManager);

        mTabSwitcherButtonCoordinator.setTabSwitcherListener(tabSwitcherListener);
        mTabSwitcherButtonCoordinator.setTabModelSelector(tabModelSelector);

        resourceManager.getDynamicResourceLoader().registerResource(
                mResourceId, mViewResourceAdapter);
    }

    /**
     * Clean up any state when the bottom toolbar is destroyed.
     */
    public void destroy() {
        mMediator.destroy();
        mTabSwitcherButtonCoordinator.destroy();
    }
}

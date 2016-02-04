// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.readermode;

import android.content.Context;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentDelegate;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentProgressObserver;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContent;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContentViewDelegate;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager.PanelPriority;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.scene_layer.ContextualSearchSceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.SceneLayer;
import org.chromium.chrome.browser.dom_distiller.DomDistillerTabUtils;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManagerDelegate;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler;
import org.chromium.components.navigation_interception.NavigationParams;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.resources.ResourceManager;

/**
 * The panel containing reader mode.
 */
public class ReaderModePanel extends OverlayPanel {

    /** The compositor layer used for drawing the panel. */
    private ContextualSearchSceneLayer mSceneLayer;

    /** Delegate for calling functions on the ReaderModeManager. */
    private ReaderModeManagerDelegate mManagerDelegate;

    /** Delegate for passing the current ContentViewCore to the layout manager. */
    private OverlayPanelContentViewDelegate mContentViewDelegate;

    /** The opacity of the panel bar text. */
    private float mReaderBarTextOpacity;

    /** If the timer for counting how long a user has been reading is running. */
    private boolean mTimerRunning;

    /** The start time in ms of the current timer. */
    private long mStartTime;

    // ============================================================================================
    // Constructor
    // ============================================================================================

    /**
     * @param context The current Android {@link Context}.
     * @param updateHost The {@link LayoutUpdateHost} used to request updates in the Layout.
     */
    public ReaderModePanel(Context context, LayoutUpdateHost updateHost,
                OverlayPanelManager panelManager,
                OverlayPanelContentViewDelegate contentViewDelegate) {
        super(context, updateHost, panelManager);
        mSceneLayer = createNewReaderModeSceneLayer();
        mContentViewDelegate = contentViewDelegate;
    }

    @Override
    public OverlayPanelContent createNewOverlayPanelContent() {
        OverlayContentDelegate delegate = new OverlayContentDelegate() {
            /**
             * Track if a navigation/load is the first one for this content.
             */
            private boolean mIsInitialLoad = true;

            @Override
            public void onContentViewCreated(ContentViewCore contentView) {
                mContentViewDelegate.setOverlayPanelContentViewCore(contentView);

                WebContents distilledWebContents = contentView.getWebContents();
                if (distilledWebContents == null) return;

                WebContents sourceWebContents = mManagerDelegate.getBasePageWebContents();
                if (sourceWebContents == null) return;

                DomDistillerTabUtils.distillAndView(sourceWebContents, distilledWebContents);
            }

            @Override
            public void onContentViewDestroyed() {
                mContentViewDelegate.releaseOverlayPanelContentViewCore();
                mIsInitialLoad = true;
            }

            @Override
            public boolean shouldInterceptNavigation(ExternalNavigationHandler externalNavHandler,
                    NavigationParams navigationParams) {
                // The initial load will be the distilled content; don't try to open a new tab if
                // this is the case. All other navigations on distilled pages will come from link
                // clicks.
                if (mIsInitialLoad) {
                    mIsInitialLoad = false;
                    return true;
                }
                if (!navigationParams.isExternalProtocol) {
                    mManagerDelegate.createNewTab(navigationParams.url);
                    return false;
                }
                return true;
            }
        };

        return new OverlayPanelContent(delegate, new OverlayContentProgressObserver(), mActivity);
    }

    // ============================================================================================
    // Scene layer
    // ============================================================================================

    @Override
    public SceneLayer getSceneLayer() {
        return mSceneLayer;
    }

    @Override
    public void updateSceneLayer(ResourceManager resourceManager) {
        if (mSceneLayer == null) return;

        // This will cause the ContentViewCore to size itself appropriately for the panel (includes
        // top controls height).
        updateTopControlsState();

        mSceneLayer.update(resourceManager, this, ContextualSearchSceneLayer.READER_MODE_PANEL,
                0, getBarTextViewId(), null, 0, mReaderBarTextOpacity, null);
    }

    /**
     * Create a new scene layer for this panel. This should be overridden by tests as necessary.
     */
    protected ContextualSearchSceneLayer createNewReaderModeSceneLayer() {
        return new ContextualSearchSceneLayer(mContext.getResources().getDisplayMetrics().density);
    }

    // ============================================================================================
    // Manager Integration
    // ============================================================================================

    /**
     * Sets the {@code ReaderModeManagerDelegate} associated with this panel.
     * @param delegate The {@code ReaderModeManagerDelegate}.
     */
    public void setManagerDelegate(ReaderModeManagerDelegate delegate) {
        // TODO(mdjones): This looks similar to setManagementDelegate in ContextualSearchPanel,
        // consider moving this to OverlayPanel.
        if (mManagerDelegate != delegate) {
            mManagerDelegate = delegate;
            if (mManagerDelegate != null) {
                setChromeActivity(mManagerDelegate.getChromeActivity());
                initializeUiState();
                // TODO(mdjones): Improve the base page movement API so that the default behavior
                // is to hide the toolbar; this function call should not be necessary here.
                updateBasePageTargetY();
            }
        }
    }

    // ============================================================================================
    // Generic Event Handling
    // ============================================================================================

    @Override
    public void handleBarClick(long time, float x, float y) {
        super.handleBarClick(time, x, y);
        if (isCoordinateInsideCloseButton(x)) {
            closePanel(StateChangeReason.CLOSE_BUTTON, true);
            mManagerDelegate.onCloseButtonPressed();
        } else {
            maximizePanel(StateChangeReason.SEARCH_BAR_TAP);
        }
    }

    @Override
    public boolean onInterceptBarClick() {
        // TODO(mdjones): Handle compatibility mode here (promote to tab on tap).
        return false;
    }

    // ============================================================================================
    // Panel base methods
    // ============================================================================================

    @Override
    public void destroyComponents() {
        super.destroyComponents();
        destroyReaderModeBarControl();
    }

    @Override
    public PanelPriority getPriority() {
        return PanelPriority.MEDIUM;
    }

    @Override
    public boolean canBeSuppressed() {
        return true;
    }

    @Override
    public boolean supportsExpandedState() {
        return false;
    }

    @Override
    protected float getThresholdToNextState() {
        return 0.30f;
    }

    @Override
    protected void updatePanelForCloseOrPeek(float percent) {
        super.updatePanelForCloseOrPeek(percent);

        // Do not update the panel text if the panel was closed immediately.
        if (percent < 0.01f) return;

        getReaderModeBarControl().setBarText(R.string.reader_view_text);
        mReaderBarTextOpacity = 1.0f;
    }

    @Override
    protected void updatePanelForMaximization(float percent) {
        super.updatePanelForMaximization(percent);
        if (percent < 0.5f) {
            mReaderBarTextOpacity = 1.0f - 2.0f * percent;
            getReaderModeBarControl().setBarText(R.string.reader_view_text);
        } else {
            mReaderBarTextOpacity = 2.0f * (percent - 0.5f);
            getReaderModeBarControl().setBarText(R.string.reader_mode_maximized_title);
        }
    }

    @Override
    protected void maximizePanel(StateChangeReason reason) {
        // Extend animation time by 150ms.
        super.animatePanelToState(PanelState.MAXIMIZED, reason, BASE_ANIMATION_DURATION_MS  + 150);
    }

    @Override
    protected void onAnimationFinished() {
        super.onAnimationFinished();
        boolean animatingToOpenState = getPanelState() == PanelState.MAXIMIZED;
        // Start or stop the timer for how long the user has been reading.
        if (!mTimerRunning && animatingToOpenState) {
            mStartTime = System.currentTimeMillis();
            mTimerRunning = true;
        } else if (mTimerRunning && !animatingToOpenState) {
            onTimerEnded();
        }
    }

    @Override
    public void closePanel(StateChangeReason reason, boolean animate) {
        super.closePanel(reason, animate);
        if (mTimerRunning) {
            onTimerEnded();
        }
    }

    /**
     * Record the time spent in Reader Mode.
     */
    private void onTimerEnded() {
        mTimerRunning = false;
        long totalTime = System.currentTimeMillis() - mStartTime;
        if (mStartTime <= 0 || totalTime < 0) return;
        mManagerDelegate.recordTimeSpentInReader(totalTime);
    }

    @Override
    public float getOffsetY() {
        // Do not attempt to auto-hide the reader mode bar if the toolbar is less than a certain
        // height.
        boolean shouldAutoHide = getToolbarHeight() >= getBarHeightPeeking();
        // This will cause the reader mode bar to behave like the top controls; sliding out of
        // view as the page scrolls.
        return super.getOffsetY() + (shouldAutoHide ? getTopControlsOffsetDp() : 0.0f);
    }

    @Override
    public float getArrowIconOpacity() {
        // TODO(mdjones): This will not be needed once Reader Mode has its own scene layer.
        // Never show the arrow icon.
        return 0.0f;
    }

    @Override
    public float getCloseIconOpacity() {
        // TODO(mdjones): Make close button controlled by overlay panel as a toggle.
        // TODO(mdjones): This will not be needed once Reader Mode has its own scene layer.
        // Always show the close icon.
        return 1.0f;
    }

    @Override
    protected float calculateBasePageTargetY(PanelState state) {
        // In the case of reader mode the base page will always need to move the same amount.
        return -getToolbarHeight();
    }

    @Override
    public void onSizeChanged(float width, float height, boolean isToolbarShowing) {
        super.onSizeChanged(width, height, isToolbarShowing);
        if (mManagerDelegate != null) {
            mManagerDelegate.onSizeChanged();
        }
        // If the panel is not closed, make sure it is in the appropriate place when the viewport
        // size changes.
        if (getPanelState() != PanelState.UNDEFINED && getPanelState() != PanelState.CLOSED) {
            resizePanelToState(getPanelState(), StateChangeReason.UNKNOWN);
        }
    }

    // ============================================================================================
    // ReaderModeBarControl
    // ============================================================================================

    private ReaderModeBarControl mReaderModeBarControl;

    /**
     * @return The Id of the Search Term View.
     */
    public int getBarTextViewId() {
        return getReaderModeBarControl().getViewId();
    }

    /**
     * Creates the ReaderModeBarControl, if needed. The Views are set to INVISIBLE, because
     * they won't actually be displayed on the screen (their snapshots will be displayed instead).
     */
    protected ReaderModeBarControl getReaderModeBarControl() {
        assert mContainerView != null;
        assert mResourceLoader != null;

        if (mReaderModeBarControl == null) {
            mReaderModeBarControl =
                    new ReaderModeBarControl(this, mContext, mContainerView, mResourceLoader);
        }

        assert mReaderModeBarControl != null;
        return mReaderModeBarControl;
    }

    /**
     * Destroys the ReaderModeBarControl.
     */
    protected void destroyReaderModeBarControl() {
        if (mReaderModeBarControl != null) {
            mReaderModeBarControl.destroy();
            mReaderModeBarControl = null;
        }
    }
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.SystemClock;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ProgressBar;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.compositor.Invalidator;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.chrome.browser.widget.SmoothProgressBar;
import org.chromium.chrome.browser.widget.TintedImageButton;
import org.chromium.ui.UiUtils;

/**
 * Layout class that contains the base shared logic for manipulating the toolbar component. For
 * interaction that are not from Views inside Toolbar hierarchy all interactions should be done
 * through {@link Toolbar} rather than using this class directly.
 */
abstract class ToolbarLayout extends FrameLayout implements Toolbar {
    protected static final int BACKGROUND_TRANSITION_DURATION_MS = 400;

    private Invalidator mInvalidator;

    private final int[] mTempPosition = new int[2];

    /**
     * The ImageButton view that represents the menu button.
     */
    protected TintedImageButton mMenuButton;
    private AppMenuButtonHelper mAppMenuButtonHelper;

    private ToolbarDataProvider mToolbarDataProvider;
    private ToolbarTabController mToolbarTabController;
    private SmoothProgressBar mProgressBar;

    private boolean mNativeLibraryReady;
    private boolean mUrlHasFocus;

    private long mFirstDrawTimeMs;

    protected final int mToolbarHeightWithoutShadow;

    /**
     * Basic constructor for {@link ToolbarLayout}.
     */
    public ToolbarLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mToolbarHeightWithoutShadow = getResources().getDimensionPixelOffset(
                getToolbarHeightWithoutShadowResId());
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mProgressBar = (SmoothProgressBar) findViewById(R.id.progress);
        if (mProgressBar != null) {
            removeView(mProgressBar);
            Drawable progressDrawable = mProgressBar.getProgressDrawable();
            getFrameLayoutParams(mProgressBar).topMargin = mToolbarHeightWithoutShadow
                    - progressDrawable.getIntrinsicHeight();
        }

        mMenuButton = (TintedImageButton) findViewById(R.id.menu_button);
        // Initialize the provider to an empty version to avoid null checking everywhere.
        mToolbarDataProvider = new ToolbarDataProvider() {
            @Override
            public boolean isIncognito() {
                return false;
            }

            @Override
            public Tab getTab() {
                return null;
            }

            @Override
            public String getText() {
                return null;
            }

            @Override
            public boolean wouldReplaceURL() {
                return false;
            }

            @Override
            public NewTabPage getNewTabPageForCurrentTab() {
                return null;
            }

            @Override
            public int getLoadProgress() {
                return 0;
            }

            @Override
            public String getCorpusChipText() {
                return null;
            }

            @Override
            public int getPrimaryColor() {
                return 0;
            }

            @Override
            public boolean isUsingBrandColor() {
                return false;
            }
        };
    }

    /**
     * Quick getter for LayoutParams for a View inside a FrameLayout.
     * @param view {@link View} to fetch the layout params for.
     * @return {@link LayoutParams} the given {@link View} is currently using.
     */
    protected FrameLayout.LayoutParams getFrameLayoutParams(View view) {
        return ((FrameLayout.LayoutParams) view.getLayoutParams());
    }

    /**
     * @return The resource id to be used while getting the toolbar height with no shadow.
     */
    protected int getToolbarHeightWithoutShadowResId() {
        return R.dimen.toolbar_height_no_shadow;
    }

    @Override
    public void initialize(ToolbarDataProvider toolbarDataProvider,
            ToolbarTabController tabController, AppMenuButtonHelper appMenuButtonHelper) {
        mToolbarDataProvider = toolbarDataProvider;
        mToolbarTabController = tabController;

        mMenuButton.setOnTouchListener(new OnTouchListener() {
            @Override
            @SuppressLint("ClickableViewAccessibility")
            public boolean onTouch(View v, MotionEvent event) {
                return mAppMenuButtonHelper.onTouch(v, event);
            }
        });
        mAppMenuButtonHelper = appMenuButtonHelper;
    }

    /**
     *  This function handles native dependent initialization for this class
     */
    public void onNativeLibraryReady() {
        mNativeLibraryReady = true;
    }

    /**
     * @return The menu button view.
     */
    protected View getMenuButton() {
        return mMenuButton;
    }

    /**
     * @return The {@link ProgressBar} this layout uses.
     */
    SmoothProgressBar getProgressBar() {
        return mProgressBar;
    }

    @Override
    public void getPositionRelativeToContainer(View containerView, int[] position) {
        ViewUtils.getRelativeDrawPosition(containerView, this, position);
    }

    /**
     * @return The helper for menu button UI interactions.
     */
    protected AppMenuButtonHelper getMenuButtonHelper() {
        return mAppMenuButtonHelper;
    }

    /**
     * @return Whether or not the native library is loaded and ready.
     */
    protected boolean isNativeLibraryReady() {
        return mNativeLibraryReady;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        recordFirstDrawTime();
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (mProgressBar != null) {
            ViewGroup controlContainer =
                    (ViewGroup) getRootView().findViewById(R.id.control_container);
            int progressBarPosition = UiUtils.insertAfter(
                    controlContainer, mProgressBar, (View) getParent());
            assert progressBarPosition >= 0;
        }
    }

    /**
     * @return The provider for toolbar related data.
     */
    protected ToolbarDataProvider getToolbarDataProvider() {
        return mToolbarDataProvider;
    }

    @Override
    public void setPaintInvalidator(Invalidator invalidator) {
        mInvalidator = invalidator;
    }

    /**
     * Triggers a paint but allows the {@link Invalidator} set by
     * {@link #setPaintInvalidator(Invalidator)} to decide when to actually invalidate.
     * @param client A {@link Invalidator.Client} instance that wants to be invalidated.
     */
    protected void triggerPaintInvalidate(Invalidator.Client client) {
        if (mInvalidator == null) {
            client.doInvalidate();
        } else {
            mInvalidator.invalidate(client);
        }
    }

    @Override
    public void setOnTabSwitcherClickHandler(OnClickListener listener) { }

    @Override
    public void setOnNewTabClickHandler(OnClickListener listener) { }

    @Override
    public void setBookmarkClickHandler(OnClickListener listener) { }

    @Override
    public void setCustomTabReturnClickHandler(OnClickListener listener) { }

    /**
     * Gives inheriting classes the chance to update the visibility of the
     * back button.
     * @param canGoBack Whether or not the current tab has any history to go back to.
     */
    protected void updateBackButtonVisibility(boolean canGoBack) { }

    /**
     * Gives inheriting classes the chance to update the visibility of the
     * forward button.
     * @param canGoForward Whether or not the current tab has any history to go forward to.
     */
    protected void updateForwardButtonVisibility(boolean canGoForward) { }

    /**
     * Gives inheriting classes the chance to update the visibility of the
     * reload button.
     * @param isReloading Whether or not the current tab is loading.
     */
    protected void updateReloadButtonVisibility(boolean isReloading) { }

    /**
     * Gives inheriting classes the chance to update the visibility of the
     * bookmark button.
     * @param isBookmarked Whether or not the current tab is already bookmarked.
     */
    protected void updateBookmarkButtonVisibility(boolean isBookmarked) { }

    /**
     * Gives inheriting classes the chance to respond to
     * {@link org.chromium.chrome.browser.widget.findinpage.FindToolbar} state changes.
     * @param showing Whether or not the {@code FindToolbar} will be showing.
     */
    protected void handleFindToolbarStateChange(boolean showing) { }

    /**
     * Gives inheriting classes the chance to respond to accessibility state changes.
     * @param enabled Whether or not accessibility is enabled.
     */
    protected void onAccessibilityStatusChanged(boolean enabled) { }

    /**
     * Gives inheriting classes the chance to do the necessary UI operations after Chrome is
     * restored to a previously saved state.
     */
    protected void onStateRestored() { }

    /**
     * Gives inheriting classes the chance to update home button UI if home button preference is
     * changed.
     * @param homeButtonEnabled Whether or not home button is enabled in preference.
     */
    protected void onHomeButtonUpdate(boolean homeButtonEnabled) { }

    /**
     * Triggered when the current tab or model has changed.
     * <p>
     * As there are cases where you can select a model with no tabs (i.e. having incognito
     * tabs but no normal tabs will still allow you to select the normal model), this should
     * not guarantee that the model's current tab is non-null.
     */
    protected void onTabOrModelChanged() {
        NewTabPage ntp = getToolbarDataProvider().getNewTabPageForCurrentTab();
        if (ntp != null) getLocationBar().onTabLoadingNTP(ntp);

        getLocationBar().updateMicButtonState();
    }

    /**
     * For extending classes to override and carry out the changes related with the primary color
     * for the current tab changing.
     */
    protected void onPrimaryColorChanged() { }

    @Override
    public void addCustomActionButton(Bitmap buttonSource, OnClickListener listener) { }

    /**
     * Triggered when the content view for the specified tab has changed.
     */
    protected void onTabContentViewChanged() {
        NewTabPage ntp = getToolbarDataProvider().getNewTabPageForCurrentTab();
        if (ntp != null) getLocationBar().onTabLoadingNTP(ntp);
    }

    @Override
    public boolean isReadyForTextureCapture() {
        return true;
    }

    /**
     * @param attached Whether or not the web content is attached to the view heirarchy.
     */
    protected void setContentAttached(boolean attached) { }

    /**
     * Gives inheriting classes the chance to show or hide the TabSwitcher mode of this toolbar.
     * @param inTabSwitcherMode Whether or not TabSwitcher mode should be shown or hidden.
     * @param showToolbar    Whether or not to show the normal toolbar while animating.
     * @param delayAnimation Whether or not to delay the animation until after the transition has
     *                       finished (which can be detected by a call to
     *                       {@link #onTabSwitcherTransitionFinished()}).
     */
    protected void setTabSwitcherMode(
            boolean inTabSwitcherMode, boolean showToolbar, boolean delayAnimation) { }

    /**
     * Gives inheriting classes the chance to update their state when the TabSwitcher transition has
     * finished.
     */
    protected void onTabSwitcherTransitionFinished() { }

    /**
     * Gives inheriting classes the chance to update themselves based on the
     * number of tabs in the current TabModel.
     * @param numberOfTabs The number of tabs in the current model.
     */
    protected void updateTabCountVisuals(int numberOfTabs) { }

    /**
     * Gives inheriting classes the chance to update themselves based on default search engine
     * changes.
     */
    protected void onDefaultSearchEngineChanged() { }

    @Override
    public void getLocationBarContentRect(Rect outRect) {
        View container = getLocationBar().getContainerView();
        outRect.set(container.getPaddingLeft(), container.getPaddingTop(),
                container.getWidth() - container.getPaddingRight(),
                container.getHeight() - container.getPaddingBottom());
        ViewUtils.getRelativeDrawPosition(
                this, getLocationBar().getContainerView(), mTempPosition);
        outRect.offset(mTempPosition[0], mTempPosition[1]);
    }

    @Override
    public void setTextureCaptureMode(boolean textureMode) { }

    @Override
    public boolean shouldIgnoreSwipeGesture() {
        return mUrlHasFocus
                || (mAppMenuButtonHelper != null && mAppMenuButtonHelper.isAppMenuActive());
    }

    /**
     * @return Whether or not the url bar has focus.
     */
    protected boolean urlHasFocus() {
        return mUrlHasFocus;
    }

    /**
     * Triggered when the URL input field has gained or lost focus.
     * @param hasFocus Whether the URL field has gained focus.
     */
    protected void onUrlFocusChange(boolean hasFocus) {
        mUrlHasFocus = hasFocus;
    }

    protected boolean shouldShowMenuButton() {
        return true;
    }

    /**
     * Keeps track of the first time the toolbar is drawn.
     */
    private void recordFirstDrawTime() {
        if (mFirstDrawTimeMs == 0) mFirstDrawTimeMs = SystemClock.elapsedRealtime();
    }

    @Override
    public long getFirstDrawTime() {
        return mFirstDrawTimeMs;
    }

    /**
     * Notified when a navigation to a different page has occurred.
     */
    protected void onNavigatedToDifferentPage() {
    }

    /**
     * Sets load progress.
     * @param progress The load progress between 0 and 100.
     */
    protected void setLoadProgress(int progress) {
        if (mProgressBar != null) mProgressBar.setProgress(progress);
    }

    @Override
    public void finishAnimations() { }

    /**
     * @return The current View showing in the Tab.
     */
    protected View getCurrentTabView() {
        Tab tab = mToolbarDataProvider.getTab();
        if (tab != null) {
            return tab.getView();
        }
        return null;
    }

    /**
     * @return Whether or not the toolbar is incognito.
     */
    protected boolean isIncognito() {
        return mToolbarDataProvider.isIncognito();
    }

    @Override
    public abstract LocationBar getLocationBar();

    /**
     * Navigates the current Tab back.
     * @return Whether or not the current Tab did go back.
     */
    protected boolean back() {
        getLocationBar().hideSuggestions();
        return mToolbarTabController != null ? mToolbarTabController.back() : false;
    }

    /**
     * Navigates the current Tab forward.
     * @return Whether or not the current Tab did go forward.
     */
    protected boolean forward() {
        getLocationBar().hideSuggestions();
        return mToolbarTabController != null ? mToolbarTabController.forward() : false;
    }

    /**
     * If the page is currently loading, this will trigger the tab to stop.  If the page is fully
     * loaded, this will trigger a refresh.
     *
     * <p>The buttons of the toolbar will be updated as a result of making this call.
     */
    protected void stopOrReloadCurrentTab() {
        getLocationBar().hideSuggestions();
        if (mToolbarTabController != null) mToolbarTabController.stopOrReloadCurrentTab();
    }

    /**
     * Opens hompage in the current tab.
     */
    protected void openHomepage() {
        getLocationBar().hideSuggestions();
        if (mToolbarTabController != null) mToolbarTabController.openHomepage();
    }
}

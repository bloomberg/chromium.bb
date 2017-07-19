// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.SystemClock;
import android.support.v7.widget.Toolbar;
import android.util.AttributeSet;
import android.util.Property;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.chrome.browser.widget.TintedImageButton;
import org.chromium.chrome.browser.widget.ToolbarProgressBar;
import org.chromium.chrome.browser.widget.animation.CancelAwareAnimatorListener;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetMetrics;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetObserver;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.interpolators.BakedBezierInterpolator;

/**
 * Phone specific toolbar that exists at the bottom of the screen.
 */
public class BottomToolbarPhone extends ToolbarPhone {
    /**
     * The observer used to listen to {@link BottomSheet} events.
     */
    private final BottomSheetObserver mBottomSheetObserver = new EmptyBottomSheetObserver() {
        @Override
        public void onSheetOpened() {
            onPrimaryColorChanged(true);
            if (mUseToolbarHandle) {
                // If the toolbar is focused, switch focus to the bottom sheet before changing the
                // content description. If the content description is changed while the view is
                // focused, the new content description is read immediately.
                if (hasFocus() && !urlHasFocus()) mBottomSheet.requestFocus();
                updateContentDescription();
            }
        }

        @Override
        public void onSheetClosed() {
            onPrimaryColorChanged(true);

            updateMenuButtonClickableState();
            updateContentDescription();
        }

        @Override
        public void onSheetReleased() {
            onPrimaryColorChanged(true);
        }

        @Override
        public void onSheetOffsetChanged(float heightFraction) {
            boolean isMovingDown = heightFraction < mLastHeightFraction;
            boolean isMovingUp = heightFraction > mLastHeightFraction;
            mLastHeightFraction = heightFraction;

            // TODO(twellington): Ideally we would wait to kick off an animation until the sheet is
            // released if we know it was opened via swipe.
            if (isMovingUp && !mAnimatingToolbarButtonDisappearance
                    && mToolbarButtonVisibilityPercent != 0.f) {
                animateToolbarButtonVisibility(false);
            } else if (isMovingDown && heightFraction < 0.40f && !mAnimatingToolbarButtonAppearance
                    && mToolbarButtonVisibilityPercent != 1.f) {
                // If the sheet is moving down and the height is less than 45% of the max, start
                // showing the toolbar buttons. 45% is used rather than 50% so that the buttons
                // aren't shown in the half height state if the user is dragging the sheet down
                // slowly and releases at exactly the half way point.
                animateToolbarButtonVisibility(true);
            }

            // The only time the omnibox should have focus is when the sheet is fully expanded. Any
            // movement of the sheet should unfocus it.
            if (isMovingDown && getLocationBar().isUrlBarFocused()) {
                getLocationBar().setUrlBarFocus(false);
            }

            boolean buttonsClickable = heightFraction == 0.f;
            mToggleTabStackButton.setClickable(buttonsClickable);
            updateMenuButtonClickableState();
            if (!mUseToolbarHandle) mExpandButton.setClickable(buttonsClickable);
        }
    };

    /**
     * A property for animating the disappearance of toolbar bar buttons. 1.f is fully visible
     * and 0.f is fully hidden.
     */
    private final Property<BottomToolbarPhone, Float> mToolbarButtonVisibilityProperty =
            new Property<BottomToolbarPhone, Float>(Float.class, "") {
                @Override
                public Float get(BottomToolbarPhone object) {
                    return object.mToolbarButtonVisibilityPercent;
                }

                @Override
                public void set(BottomToolbarPhone object, Float value) {
                    object.mToolbarButtonVisibilityPercent = value;
                    if (!mUrlFocusChangeInProgress) updateToolbarButtonVisibility();
                }
            };

    /** The background alpha for the tab switcher. */
    private static final float TAB_SWITCHER_TOOLBAR_ALPHA = 0.7f;

    /** The white version of the toolbar handle; used for dark themes and incognito. */
    private final Drawable mHandleLight;

    /** The dark version of the toolbar handle; this is the default handle to use. */
    private final Drawable mHandleDark;

    /** A handle to the bottom sheet. */
    private BottomSheet mBottomSheet;

    /** A handle to the expand button that Chrome Home may or may not use. */
    private TintedImageButton mExpandButton;

    /**
     * Whether some of the toolbar buttons are hidden regardless of whether the URL bar is focused.
     * If {@link #mShowMenuButtonWhenSheetOpen} is false, all buttons are hidden.
     * If {@link #mShowMenuButtonWhenSheetOpen} is true, all buttons besides the menu button are
     * hidden.
     */
    private boolean mHidingSomeToolbarButtons;

    /**
     * This tracks the height fraction of the bottom bar to determine if it is moving up or down.
     */
    private float mLastHeightFraction;

    /** The toolbar handle view that indicates the toolbar can be pulled upward. */
    private ImageView mToolbarHandleView;

    /** Whether or not the toolbar handle should be used. */
    private boolean mUseToolbarHandle;

    /**
     * Tracks whether the toolbar buttons are hidden, with 1.f being fully visible and 0.f being
     * fully hidden.
     */
    private float mToolbarButtonVisibilityPercent;

    /** Animates toolbar button visibility. */
    private Animator mToolbarButtonVisibilityAnimator;

    /** Whether the appearance of the toolbar buttons is currently animating. */
    private boolean mAnimatingToolbarButtonAppearance;

    /** Whether the disappearance of the toolbar buttons is currently animating. */
    private boolean mAnimatingToolbarButtonDisappearance;

    /** Whether the menu button should be shown while the sheet is open. */
    private boolean mShowMenuButtonWhenSheetOpen;

    /**
     * Constructs a BottomToolbarPhone object.
     * @param context The Context in which this View object is created.
     * @param attrs The AttributeSet that was specified with this View.
     */
    public BottomToolbarPhone(Context context, AttributeSet attrs) {
        super(context, attrs);

        mHandleDark = ApiCompatibilityUtils.getDrawable(
                context.getResources(), R.drawable.toolbar_handle_dark);
        mHandleLight = ApiCompatibilityUtils.getDrawable(
                context.getResources(), R.drawable.toolbar_handle_light);
        mLocationBarVerticalMargin =
                getResources().getDimensionPixelOffset(R.dimen.bottom_location_bar_vertical_margin);
        mUseToolbarHandle = true;
        mToolbarButtonVisibilityPercent = 1.f;
    }

    /**
     * @return The expand button if it is being used.
     */
    public View getExpandButton() {
        return mExpandButton;
    }

    /**
     * Set the color of the pull handle used by the toolbar.
     * @param useLightDrawable If the handle color should be light.
     */
    public void updateHandleTint(boolean useLightDrawable) {
        if (!mUseToolbarHandle) return;
        mToolbarHandleView.setImageDrawable(useLightDrawable ? mHandleLight : mHandleDark);
    }

    /**
     * @return Whether or not the toolbar is currently using a light theme color.
     */
    public boolean isLightTheme() {
        return !ColorUtils.shouldUseLightForegroundOnBackground(getTabThemeColor());
    }

    @Override
    public boolean isInTabSwitcherMode() {
        return !mBottomSheet.isSheetOpen() && super.isInTabSwitcherMode();
    }

    @Override
    protected boolean shouldDrawShadow() {
        return mBottomSheet.isSheetOpen() || super.shouldDrawShadow();
    }

    @Override
    public boolean isReadyForTextureCapture() {
        return super.isReadyForTextureCapture() && !mBottomSheet.isShowingNewTab();
    }

    /** Shows the tab switcher toolbar. */
    public void showTabSwitcherToolbar() {
        setTabSwitcherMode(true, true, false);
    }

    /** Shows the normal toolbar. */
    public void showNormalToolbar() {
        // TODO(twellington): Add animation.
        setTabSwitcherMode(false, true, false, false);

        // Typically #onTabSwitcherTransitionFinished() is called when the tab switcher is finished
        // hiding. In this scenario, however, we are showing the normal toolbar without hiding
        // the tab switcher. Call #onTabSwitcherTransitionFinished() directly so that ToolbarPhone
        // updates its state.
        onTabSwitcherTransitionFinished();
    }

    @Override
    protected void setTabSwitcherMode(boolean inTabSwitcherMode, boolean showToolbar,
            boolean delayAnimation, boolean animate) {
        super.setTabSwitcherMode(inTabSwitcherMode, showToolbar, delayAnimation, animate);
        if (!mUseToolbarHandle) mExpandButton.setClickable(!inTabSwitcherMode);
        updateContentDescription();
    }

    @Override
    protected void onTabSwitcherTransitionFinished() {
        super.onTabSwitcherTransitionFinished();
        updateContentDescription();
    }

    @Override
    protected int getProgressBarColor() {
        int color = super.getProgressBarColor();
        if (getToolbarDataProvider().getTab() != null) {
            // ToolbarDataProvider itself accounts for Chrome Home and will return default colors,
            // so pull the progress bar color from the tab.
            color = getToolbarDataProvider().getTab().getThemeColor();
        }
        return color;
    }

    @Override
    protected int getProgressBarTopMargin() {
        // In the case where the toolbar is at the bottom of the screen, the progress bar should
        // be at the top of the screen.
        return 0;
    }

    @Override
    protected int getProgressBarHeight() {
        return getResources().getDimensionPixelSize(R.dimen.chrome_home_progress_bar_height);
    }

    @Override
    protected ToolbarProgressBar createProgressBar() {
        return new ToolbarProgressBar(
                getContext(), getProgressBarHeight(), getProgressBarTopMargin(), true);
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        Tab currentTab = getToolbarDataProvider().getTab();
        if (currentTab != null) {
            currentTab.getActivity().getBottomSheetContentController().onOmniboxFocusChange(
                    hasFocus);
        }

        if (mToolbarButtonVisibilityAnimator != null
                && mToolbarButtonVisibilityAnimator.isRunning()) {
            mToolbarButtonVisibilityAnimator.end();
        }

        super.onUrlFocusChange(hasFocus);
    }

    @Override
    protected void triggerUrlFocusAnimation(final boolean hasFocus) {
        super.triggerUrlFocusAnimation(hasFocus);

        if (mBottomSheet == null || !hasFocus) return;

        boolean wasSheetOpen = mBottomSheet.isSheetOpen();
        mBottomSheet.setSheetState(BottomSheet.SHEET_STATE_FULL, true);

        if (!wasSheetOpen) {
            mBottomSheet.getBottomSheetMetrics().recordSheetOpenReason(
                    BottomSheetMetrics.OPENED_BY_OMNIBOX_FOCUS);
        }
    }

    @Override
    public void setBottomSheet(BottomSheet sheet) {
        assert mBottomSheet == null;

        mBottomSheet = sheet;
        getLocationBar().setBottomSheet(mBottomSheet);
        mBottomSheet.addObserver(mBottomSheetObserver);
    }

    @Override
    public boolean shouldIgnoreSwipeGesture() {
        // Only detect swipes if the bottom sheet in the peeking state and not animating.
        return mBottomSheet.getSheetState() != BottomSheet.SHEET_STATE_PEEK
                || mBottomSheet.isRunningSettleAnimation() || super.shouldIgnoreSwipeGesture();
    }

    @Override
    protected void addProgressBarToHierarchy() {
        if (mProgressBar == null) return;

        ViewGroup coordinator = (ViewGroup) getRootView().findViewById(R.id.coordinator);
        UiUtils.insertBefore(coordinator, mProgressBar, mBottomSheet);

        mProgressBar.setProgressBarContainer(coordinator);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        if (!mDisableLocationBarRelayout && !isInTabSwitcherMode()
                && (mAnimatingToolbarButtonAppearance || mAnimatingToolbarButtonDisappearance)) {
            // ToolbarPhone calls #updateUrlExpansionAnimation() in its #onMeasure(). If the toolbar
            // button visibility animation is running, call #updateToolbarButtonVisibility() to
            // ensure that view properties are set correctly.
            updateToolbarButtonVisibility();
        }
    }

    /**
     * @return The extra top margin that should be applied to the browser controls views to
     *         correctly offset them from the handle that sits above them.
     */
    private int getExtraTopMargin() {
        if (!mUseToolbarHandle) return 0;
        return getResources().getDimensionPixelSize(R.dimen.bottom_toolbar_top_margin);
    }

    @Override
    protected int getLeftPositionOfLocationBarBackground(VisualState visualState) {
        if (!mAnimatingToolbarButtonAppearance && !mAnimatingToolbarButtonDisappearance) {
            return super.getLeftPositionOfLocationBarBackground(visualState);
        } else {
            int targetPosition = getViewBoundsLeftOfLocationBar(visualState);
            int currentPosition = targetPosition + getLocationBarBackgroundLeftOffset();
            return (int) MathUtils.interpolate(
                    targetPosition, currentPosition, mToolbarButtonVisibilityPercent);
        }
    }

    @Override
    protected int getRightPositionOfLocationBarBackground(VisualState visualState) {
        if (!mAnimatingToolbarButtonAppearance && !mAnimatingToolbarButtonDisappearance) {
            return super.getRightPositionOfLocationBarBackground(visualState);
        } else {
            int targetPosition = getViewBoundsRightOfLocationBar(visualState);
            int currentPosition = targetPosition - getLocationBarBackgroundRightOffset();
            return (int) MathUtils.interpolate(
                    targetPosition, currentPosition, mToolbarButtonVisibilityPercent);
        }
    }

    private int getToolbarButtonsWidthForBackgroundOffset() {
        return mShowMenuButtonWhenSheetOpen
                ? mToolbarButtonsContainer.getMeasuredWidth() - mMenuButton.getMeasuredWidth()
                : mToolbarButtonsContainer.getMeasuredWidth();
    }

    private int getLocationBarBackgroundLeftOffset() {
        return !ApiCompatibilityUtils.isLayoutRtl(this)
                ? 0
                : getToolbarButtonsWidthForBackgroundOffset() - mToolbarSidePadding;
    }

    private int getLocationBarBackgroundRightOffset() {
        return !ApiCompatibilityUtils.isLayoutRtl(this)
                ? getToolbarButtonsWidthForBackgroundOffset() - mToolbarSidePadding
                : 0;
    }

    @Override
    protected int getBoundsAfterAccountingForRightButtons() {
        if (!mHidingSomeToolbarButtons) return super.getBoundsAfterAccountingForRightButtons();

        return !mShowMenuButtonWhenSheetOpen ? mToolbarSidePadding
                                             : mMenuButton.getMeasuredWidth() + mToolbarSidePadding;
    }

    @Override
    protected boolean isChildLeft(View child) {
        return (child == mNewTabButton || child == mExpandButton) ^ LocalizationUtils.isLayoutRtl();
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();

        mExpandButton = (TintedImageButton) findViewById(R.id.expand_sheet_button);

        // Add extra top margin to the URL bar to compensate for the change to location bar's
        // vertical margin in the constructor.
        ((MarginLayoutParams) mLocationBar.findViewById(R.id.url_bar).getLayoutParams()).topMargin =
                getResources().getDimensionPixelSize(R.dimen.bottom_toolbar_url_bar_top_margin);

        // Exclude the location bar from the list of browsing mode views. This prevents its
        // visibility from changing during transitions.
        mBrowsingModeViews.remove(mLocationBar);

        updateToolbarTopMargin();
    }

    @Override
    public void initialize(ToolbarDataProvider toolbarDataProvider,
            ToolbarTabController tabController, AppMenuButtonHelper appMenuButtonHelper) {
        super.initialize(toolbarDataProvider, tabController, appMenuButtonHelper);
        mAppMenuButtonHelper.setShowMenuOnUp(true);
    }

    /**
     * Update the top margin of all the components inside the toolbar. If the toolbar handle is
     * being used, extra margin is added.
     */
    private void updateToolbarTopMargin() {
        // Programmatically apply a top margin to all the children of the toolbar container. This
        // is done so the view hierarchy does not need to be changed.
        int topMarginForControls = getExtraTopMargin();

        View topShadow = findViewById(R.id.bottom_toolbar_shadow);

        for (int i = 0; i < getChildCount(); i++) {
            View curView = getChildAt(i);

            // Skip the shadow that sits at the top of the toolbar since this needs to sit on top
            // of the toolbar.
            if (curView == topShadow) continue;

            ((MarginLayoutParams) curView.getLayoutParams()).topMargin = topMarginForControls;
        }
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();

        // The toolbar handle is part of the control container so it can draw on top of the
        // other toolbar views, this way there is only a single handle instead of each having its
        // own. Get the root view and search for the handle.
        mToolbarHandleView = (ImageView) getRootView().findViewById(R.id.toolbar_handle);
        mToolbarHandleView.setImageDrawable(mHandleDark);
    }

    @Override
    public void onNativeLibraryReady() {
        super.onNativeLibraryReady();

        mUseToolbarHandle = !FeatureUtilities.isChromeHomeExpandButtonEnabled();

        if (!mUseToolbarHandle) {
            initExpandButton();
        } else {
            updateContentDescription();
        }
    }

    /**
     * Initialize the "expand" button if it is being used.
     */
    private void initExpandButton() {
        mLocationBarVerticalMargin =
                getResources().getDimensionPixelOffset(R.dimen.location_bar_vertical_margin);

        mToolbarHandleView.setVisibility(View.GONE);

        mExpandButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mBottomSheet != null && mTabSwitcherState == STATIC_TAB) {
                    mBottomSheet.onExpandButtonPressed();
                }
            }
        });

        mExpandButton.setVisibility(View.VISIBLE);

        updateToolbarTopMargin();
    }

    @Override
    protected void updateVisualsForToolbarState() {
        super.updateVisualsForToolbarState();

        getProgressBar().setThemeColor(getProgressBarColor(), isIncognito());

        // TODO(mdjones): Creating a new tab from the tab switcher skips the
        // drawTabSwitcherFadeAnimation which would otherwise make this line unnecessary.
        if (mTabSwitcherState == STATIC_TAB && mUseToolbarHandle) mToolbarHandleView.setAlpha(1f);

        // The tab switcher's background color should not affect the toolbar handle; it should only
        // switch color based on the static tab's theme color. This is done so fade in/out looks
        // correct.
        if (mUseToolbarHandle) {
            mToolbarHandleView.setImageDrawable(isLightTheme() ? mHandleDark : mHandleLight);
        } else {
            ColorStateList tint = isIncognito() ? mLightModeTint : mDarkModeTint;
            mExpandButton.setTint(tint);
        }

        if (mBottomSheet.isSheetOpen()) {
            mShowMenuButtonWhenSheetOpen = mBottomSheet.isShowingNewTab();
            updateButtonsContainerVisibilityAndTranslation();
            updateMenuButtonClickableState();
        }
    }

    @Override
    protected void onPrimaryColorChanged(boolean shouldAnimate) {
        // Intentionally not calling super to avoid needless work.
        getProgressBar().setThemeColor(getProgressBarColor(), isIncognito());
    }

    @Override
    protected void updateLocationBarBackgroundBounds(Rect out, VisualState visualState) {
        super.updateLocationBarBackgroundBounds(out, visualState);

        // Allow the location bar to expand to the full height of the control container.
        out.top -= getExtraTopMargin() * mUrlExpansionPercent;
    }

    @Override
    protected boolean shouldDrawLocationBar() {
        return true;
    }

    @Override
    protected void drawTabSwitcherFadeAnimation(boolean animationFinished, float progress) {
        mNewTabButton.setAlpha(progress);

        mLocationBar.setAlpha(1f - progress);
        mLocationBar.setVisibility(MathUtils.areFloatsEqual(mLocationBar.getAlpha(), 0f)
                        ? View.INVISIBLE
                        : View.VISIBLE);

        if (mUseToolbarHandle) {
            mToolbarHandleView.setAlpha(1f - progress);
        } else {
            mExpandButton.setAlpha(1f - progress);
        }

        int tabSwitcherThemeColor = getToolbarColorForVisualState(VisualState.TAB_SWITCHER_NORMAL);

        updateToolbarBackground(ColorUtils.getColorWithOverlay(
                getTabThemeColor(), tabSwitcherThemeColor, progress));

        // Don't use transparency for accessibility mode or low-end devices since the
        // {@link OverviewListLayout} will be used instead of the normal tab switcher.
        if (!DeviceClassManager.enableAccessibilityLayout()) {
            float alphaTransition = 1f - TAB_SWITCHER_TOOLBAR_ALPHA;
            mToolbarBackground.setAlpha((int) ((1f - (alphaTransition * progress)) * 255));
        }
    }

    @Override
    public void finishAnimations() {
        super.finishAnimations();
        drawTabSwitcherFadeAnimation(true, mTabSwitcherModePercent);
    }

    @Override
    protected void drawTabSwitcherAnimationOverlay(Canvas canvas, float animationProgress) {
        // Intentionally overridden to block everything but the compositor screen shot. Otherwise
        // the toolbar in Chrome Home does not have an animation overlay component.
        if (mTextureCaptureMode) {
            super.drawTabSwitcherAnimationOverlay(canvas, 0f);
            if (!mUseToolbarHandle && mExpandButton.getVisibility() != View.GONE) {
                canvas.save();
                translateCanvasToView(this, mToolbarButtonsContainer, canvas);
                drawChild(canvas, mExpandButton, SystemClock.uptimeMillis());
                canvas.restore();
            }
        }
    }

    @Override
    protected void resetNtpAnimationValues() {
        // The NTP animations don't matter if the browser is in tab switcher mode.
        if (mTabSwitcherState != ToolbarPhone.STATIC_TAB) return;
        super.resetNtpAnimationValues();
    }

    @Override
    protected void updateToolbarBackground(VisualState visualState) {
        if (visualState == VisualState.TAB_SWITCHER_NORMAL
                || visualState == VisualState.TAB_SWITCHER_INCOGNITO) {
            // drawTabSwitcherFadeAnimation will handle the background color transition.
            return;
        }

        super.updateToolbarBackground(visualState);
    }

    @Override
    protected int getToolbarButtonVisibility() {
        if (mUrlExpansionPercent == 1f) return INVISIBLE;
        if (mShowMenuButtonWhenSheetOpen) return VISIBLE;
        if (mHidingSomeToolbarButtons) return INVISIBLE;
        return VISIBLE;
    }

    @Override
    protected float getUrlActionsTranslationXForExpansionAnimation(
            boolean isLocationBarRtl, boolean isRtl, float locationBarBaseTranslationX) {
        if (!mHidingSomeToolbarButtons) {
            return super.getUrlActionsTranslationXForExpansionAnimation(
                    isLocationBarRtl, isRtl, locationBarBaseTranslationX);
        }

        float urlActionsTranslationX = 0;
        // When the end toolbar buttons are not hidden, URL actions are shown and hidden due to
        // a change in location bar's width. When the end toolbar buttons are hidden, the
        // location bar's width does not change by as much, causing the end location for the URL
        // actions to be immediately visible. Translate the URL action container so that their
        // appearance is animated.
        float urlActionsTranslationXOffset =
                mUrlActionContainer.getWidth() * (1 - mUrlExpansionPercent);
        if (isLocationBarRtl) {
            urlActionsTranslationX -= urlActionsTranslationXOffset;
        } else {
            urlActionsTranslationX += urlActionsTranslationXOffset;
        }
        return urlActionsTranslationX;
    }

    @Override
    protected void onHomeButtonUpdate(boolean homeButtonEnabled) {
        // Intentionally does not call super. Chrome Home does not support a home button.
    }

    /**
     * Sets the height and title text appearance of the provided toolbar so that its style is
     * consistent with BottomToolbarPhone.
     * @param otherToolbar The other {@link Toolbar} to style.
     */
    public void setOtherToolbarStyle(Toolbar otherToolbar) {
        // Android's Toolbar class typically changes its height based on device orientation.
        // BottomToolbarPhone has a fixed height. Update |toolbar| to match.
        otherToolbar.getLayoutParams().height = getHeight();

        // Android Toolbar action buttons are aligned based on the minimum height.
        int extraTopMargin = getExtraTopMargin();
        otherToolbar.setMinimumHeight(getHeight() - extraTopMargin);

        otherToolbar.setTitleTextAppearance(
                otherToolbar.getContext(), R.style.BottomSheetContentTitle);
        ApiCompatibilityUtils.setPaddingRelative(otherToolbar,
                ApiCompatibilityUtils.getPaddingStart(otherToolbar),
                otherToolbar.getPaddingTop() + extraTopMargin,
                ApiCompatibilityUtils.getPaddingEnd(otherToolbar), otherToolbar.getPaddingBottom());

        otherToolbar.requestLayout();
    }

    private void animateToolbarButtonVisibility(final boolean visible) {
        if (mToolbarButtonVisibilityAnimator != null
                && mToolbarButtonVisibilityAnimator.isRunning()) {
            mToolbarButtonVisibilityAnimator.cancel();
            mToolbarButtonVisibilityAnimator = null;
        }

        if (mUrlFocusChangeInProgress) {
            if (visible) {
                mHidingSomeToolbarButtons = false;
                mShowMenuButtonWhenSheetOpen = false;
                mToolbarButtonVisibilityPercent = 1.f;

                mToolbarButtonsContainer.setAlpha(1.f);
                mToolbarButtonsContainer.setVisibility(View.VISIBLE);
                mToolbarButtonsContainer.setTranslationX(0);

                mToggleTabStackButton.setAlpha(1.f);
                mToggleTabStackButton.setVisibility(View.VISIBLE);

                if (!mUseToolbarHandle) {
                    if (mTabSwitcherState != ENTERING_TAB_SWITCHER) mExpandButton.setAlpha(1.f);
                    mExpandButton.setVisibility(View.VISIBLE);
                }

                requestLayout();
            } else {
                mToolbarButtonVisibilityPercent = 0.f;
                // Wait to set mShouldHideToolbarButtons until URL focus finishes.
            }

            return;
        }

        mToolbarButtonVisibilityAnimator = ObjectAnimator.ofFloat(
                BottomToolbarPhone.this, mToolbarButtonVisibilityProperty, visible ? 1.f : 0.f);

        mToolbarButtonVisibilityAnimator.setDuration(BottomSheet.BASE_ANIMATION_DURATION_MS);
        mToolbarButtonVisibilityAnimator.setInterpolator(visible
                        ? BakedBezierInterpolator.FADE_IN_CURVE
                        : BakedBezierInterpolator.FADE_OUT_CURVE);

        mToolbarButtonVisibilityAnimator.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onStart(Animator animation) {
                mAnimatingToolbarButtonDisappearance = !visible;
                mAnimatingToolbarButtonAppearance = visible;

                if (!visible) {
                    mShowMenuButtonWhenSheetOpen = mBottomSheet.isShowingNewTab();
                    mHidingSomeToolbarButtons = true;
                    mLayoutLocationBarInFocusedMode = true;
                    requestLayout();
                } else {
                    mDisableLocationBarRelayout = true;
                }
            }

            @Override
            public void onCancel(Animator animation) {
                if (visible) mDisableLocationBarRelayout = false;

                mAnimatingToolbarButtonDisappearance = false;
                mAnimatingToolbarButtonAppearance = false;
                mToolbarButtonVisibilityAnimator = null;
            }

            @Override
            public void onEnd(Animator animation) {
                if (visible) {
                    mHidingSomeToolbarButtons = false;
                    mDisableLocationBarRelayout = false;
                    mLayoutLocationBarInFocusedMode = false;
                    mShowMenuButtonWhenSheetOpen = false;
                    requestLayout();
                }

                mAnimatingToolbarButtonDisappearance = false;
                mAnimatingToolbarButtonAppearance = false;
                mToolbarButtonVisibilityAnimator = null;
            }
        });

        mToolbarButtonVisibilityAnimator.start();
    }

    @Override
    protected void onUrlFocusChangeAnimationFinished() {
        if (urlHasFocus()) {
            mHidingSomeToolbarButtons = true;
            mToolbarButtonVisibilityPercent = 0.f;
            updateButtonsContainerVisibilityAndTranslation();
        }
        updateMenuButtonClickableState();
    }

    private void updateToolbarButtonVisibility() {
        boolean isRtl = ApiCompatibilityUtils.isLayoutRtl(this);

        updateButtonsContainerVisibilityAndTranslation();

        float locationBarTranslationX;
        boolean isLocationBarRtl = ApiCompatibilityUtils.isLayoutRtl(mLocationBar);
        FrameLayout.LayoutParams locationBarLayoutParams = getFrameLayoutParams(mLocationBar);
        int currentLeftMargin = locationBarLayoutParams.leftMargin;
        int currentWidth = locationBarLayoutParams.width;

        if (isLocationBarRtl) {
            // The location bar contents should be aligned with the right side of the toolbar.
            // If RTL text is displayed in an LTR toolbar, the right position of the location bar
            // background will change as the location bar background expands/contracts.
            locationBarTranslationX =
                    -currentWidth + getRightPositionOfLocationBarBackground(mVisualState);

            if (!mHasVisibleViewPriorToUrlBar) locationBarTranslationX -= mToolbarSidePadding;
        } else {
            // The location bar contents should be aligned with the left side of the location bar
            // background. If LTR text is displayed in an RTL toolbar, the current left position of
            // the location bar background will change as the location bar background
            // expands/contracts.
            locationBarTranslationX = mUnfocusedLocationBarLayoutLeft
                    + getLeftPositionOfLocationBarBackground(mVisualState) - mToolbarSidePadding;
        }

        locationBarTranslationX -= currentLeftMargin;

        // Get the padding straight from the location bar instead of
        // |mLocationBarBackgroundPadding|, because it might be different in incognito mode.
        if (isRtl) {
            locationBarTranslationX -= mLocationBar.getPaddingRight();
        } else {
            locationBarTranslationX += mLocationBar.getPaddingLeft();
        }

        mLocationBar.setTranslationX(locationBarTranslationX);

        // Force an invalidation of the location bar to properly handle the clipping of the URL
        // bar text as a result of the bounds changing.
        mLocationBar.invalidate();
        invalidate();
    }

    /**
     * Updates the visibility, alpha and translation of the buttons container based on
     * {@link #mToolbarButtonVisibilityPercent}. If {@link #mShowMenuButtonWhenSheetOpen} is true,
     * the tab switcher button and, if present, the expand button are faded out; nothing is
     * translated. If {@link #mShowMenuButtonWhenSheetOpen} is false, the entire
     * {@link #mToolbarButtonsContainer} is faded out and translated so that the buttons appear to
     * slide off the toolbar.
     */
    private void updateButtonsContainerVisibilityAndTranslation() {
        if (mShowMenuButtonWhenSheetOpen) {
            mToolbarButtonsContainer.setTranslationX(0);
            mToolbarButtonsContainer.setAlpha(1.f);
            mToolbarButtonsContainer.setVisibility(View.VISIBLE);

            float buttonAlpha = mToolbarButtonVisibilityPercent <= 0.5
                    ? 0
                    : 1.f - ((1.f - mToolbarButtonVisibilityPercent) * 2);
            mToggleTabStackButton.setAlpha(buttonAlpha);
            mToggleTabStackButton.setVisibility(
                    mToolbarButtonVisibilityPercent > 0.f ? View.VISIBLE : View.INVISIBLE);

            if (!mUseToolbarHandle) {
                if (mTabSwitcherState != ENTERING_TAB_SWITCHER) mExpandButton.setAlpha(buttonAlpha);
                mExpandButton.setVisibility(
                        mToolbarButtonVisibilityPercent > 0.f ? View.VISIBLE : View.INVISIBLE);
            }
        } else {
            mToggleTabStackButton.setAlpha(1.f);
            mToggleTabStackButton.setVisibility(View.VISIBLE);

            if (!mUseToolbarHandle) {
                if (mTabSwitcherState != ENTERING_TAB_SWITCHER) mExpandButton.setAlpha(1.f);
                mExpandButton.setVisibility(View.VISIBLE);
            }

            boolean isRtl = ApiCompatibilityUtils.isLayoutRtl(this);

            float toolbarButtonsContainerWidth = mToolbarButtonsContainer.getMeasuredWidth();
            float toolbarButtonsTranslationX =
                    toolbarButtonsContainerWidth * (1.f - mToolbarButtonVisibilityPercent);
            if (isRtl) toolbarButtonsTranslationX *= -1;

            mToolbarButtonsContainer.setTranslationX(toolbarButtonsTranslationX);
            mToolbarButtonsContainer.setAlpha(mToolbarButtonVisibilityPercent);
            mToolbarButtonsContainer.setVisibility(
                    mToolbarButtonVisibilityPercent > 0.f ? View.VISIBLE : View.INVISIBLE);
        }
    }

    private void updateMenuButtonClickableState() {
        mMenuButton.setClickable(
                !urlHasFocus() && (!mBottomSheet.isSheetOpen() || mBottomSheet.isShowingNewTab()));
    }

    private void updateContentDescription() {
        if (!mUseToolbarHandle) return;

        if (isInTabSwitcherMode()) {
            setContentDescription(null);
        } else if (mBottomSheet.isSheetOpen()) {
            setContentDescription(
                    getResources().getString(R.string.bottom_sheet_open_accessibility_toolbar));
        } else {
            setContentDescription(
                    getResources().getString(R.string.bottom_sheet_accessibility_toolbar));
        }
    }
}

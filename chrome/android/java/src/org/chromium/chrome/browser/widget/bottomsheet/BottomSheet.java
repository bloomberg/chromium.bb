// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.Region;
import android.net.Uri;
import android.os.Build;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.Interpolator;
import android.widget.FrameLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.SysUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.NativePageHost;
import org.chromium.chrome.browser.TabLoadStatus;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager.FullscreenListener;
import org.chromium.chrome.browser.ntp.NativePageFactory;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ActionModeController.ActionBarDelegate;
import org.chromium.chrome.browser.toolbar.BottomToolbarPhone;
import org.chromium.chrome.browser.toolbar.ViewShiftingActionBarDelegate;
import org.chromium.chrome.browser.util.AccessibilityUtil;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.chrome.browser.widget.FadingBackgroundView;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContentController.ContentType;
import org.chromium.chrome.browser.widget.textbubble.TextBubble;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.BrowserControlsState;
import org.chromium.ui.UiUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * This class defines the bottom sheet that has multiple states and a persistently showing toolbar.
 * Namely, the states are:
 * - PEEK: Only the toolbar is visible at the bottom of the screen.
 * - HALF: The sheet is expanded to consume around half of the screen.
 * - FULL: The sheet is expanded to its full height.
 *
 * All the computation in this file is based off of the bottom of the screen instead of the top
 * for simplicity. This means that the bottom of the screen is 0 on the Y axis.
 */
public class BottomSheet
        extends FrameLayout implements BottomSheetSwipeDetector.SwipeableBottomSheet,
                                       FadingBackgroundView.FadingViewObserver, NativePageHost {
    /** The different states that the bottom sheet can have. */
    @IntDef({SHEET_STATE_NONE, SHEET_STATE_PEEK, SHEET_STATE_HALF, SHEET_STATE_FULL,
            SHEET_STATE_SCROLLING})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SheetState {}
    /**
     * SHEET_STATE_NONE is for internal use only and indicates the sheet is not currently
     * transitioning between states.
     */
    private static final int SHEET_STATE_NONE = -1;
    public static final int SHEET_STATE_PEEK = 0;
    public static final int SHEET_STATE_HALF = 1;
    public static final int SHEET_STATE_FULL = 2;
    public static final int SHEET_STATE_SCROLLING = 3;

    /** The different reasons that the sheet's state can change. */
    @IntDef({StateChangeReason.NONE, StateChangeReason.OMNIBOX_FOCUS, StateChangeReason.SWIPE,
            StateChangeReason.NEW_TAB, StateChangeReason.EXPAND_BUTTON, StateChangeReason.STARTUP,
            StateChangeReason.BACK_PRESS, StateChangeReason.TAP_SCRIM,
            StateChangeReason.NAVIGATION})
    @Retention(RetentionPolicy.SOURCE)
    public @interface StateChangeReason {
        int NONE = 0;
        int OMNIBOX_FOCUS = 1;
        int SWIPE = 2;
        int NEW_TAB = 3;
        int EXPAND_BUTTON = 4;
        int STARTUP = 5;
        int BACK_PRESS = 6;
        int TAP_SCRIM = 7;
        int NAVIGATION = 8;
    }

    /**
     * A specialized FrameLayout that is capable of ignoring all user input based on the state of
     * the bottom sheet.
     */
    public static class TouchRestrictingFrameLayout extends FrameLayout {
        /** A handle to the bottom sheet. */
        private BottomSheet mBottomSheet;

        public TouchRestrictingFrameLayout(Context context, AttributeSet atts) {
            super(context, atts);
        }

        /**
         * @param sheet The bottom sheet.
         */
        public void setBottomSheet(BottomSheet sheet) {
            mBottomSheet = sheet;
        }

        /**
         * @return Whether touch is enabled.
         */
        private boolean isTouchDisabled() {
            return mBottomSheet == null || mBottomSheet.isRunningContentSwapAnimation()
                    || mBottomSheet.getSheetState() == BottomSheet.SHEET_STATE_SCROLLING;
        }

        @Override
        public boolean onInterceptTouchEvent(MotionEvent event) {
            if (isTouchDisabled()) return false;
            return super.onInterceptTouchEvent(event);
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            if (isTouchDisabled()) return false;
            return super.onTouchEvent(event);
        }
    }

    /**
     * The base duration of the settling animation of the sheet. 218 ms is a spec for material
     * design (this is the minimum time a user is guaranteed to pay attention to something).
     */
    public static final long BASE_ANIMATION_DURATION_MS = 218;

    /** The amount of time it takes to transition sheet content in or out. */
    private static final long TRANSITION_DURATION_MS = 150;

    /**
     * The fraction of the way to the next state the sheet must be swiped to animate there when
     * released. This is the value used when there are 3 active states. A smaller value here means
     * a smaller swipe is needed to move the sheet around.
     */
    private static final float THRESHOLD_TO_NEXT_STATE_3 = 0.5f;

    /** This is similar to {@link #THRESHOLD_TO_NEXT_STATE_3} but for 2 states instead of 3. */
    private static final float THRESHOLD_TO_NEXT_STATE_2 = 0.3f;

    /** The height ratio for the sheet in the SHEET_STATE_HALF state. */
    private static final float HALF_HEIGHT_RATIO = 0.55f;

    /** The fraction of the width of the screen that, when swiped, will cause the sheet to move. */
    private static final float SWIPE_ALLOWED_FRACTION = 0.2f;

    /**
     * The minimum swipe velocity (dp/ms) that should be considered as a user opening the bottom
     * sheet intentionally. This is specifically for the 'velocity' swipe logic.
     */
    private static final float SHEET_SWIPE_MIN_DP_PER_MS = 0.2f;

    /**
     * Information about the different scroll states of the sheet. Order is important for these,
     * they go from smallest to largest.
     */
    private static final int[] sStates =
            new int[] {SHEET_STATE_PEEK, SHEET_STATE_HALF, SHEET_STATE_FULL};
    private final float[] mStateRatios = new float[3];

    /** The interpolator that the height animator uses. */
    private final Interpolator mInterpolator = new DecelerateInterpolator(1.0f);

    /** The list of observers of this sheet. */
    private final ObserverList<BottomSheetObserver> mObservers = new ObserverList<>();

    /** The visible rect for the screen taking the keyboard into account. */
    private final Rect mVisibleViewportRect = new Rect();

    /** The minimum distance between half and full states to allow the half state. */
    private final float mMinHalfFullDistance;

    /** The height of the shadow that sits above the toolbar. */
    private final int mToolbarShadowHeight;

    /** The {@link BottomSheetMetrics} used to record user actions and histograms. */
    private final BottomSheetMetrics mMetrics;

    /** The {@link BottomSheetNewTabController} used to present the new tab UI. */
    private BottomSheetNewTabController mNtpController;

    /** For detecting scroll and fling events on the bottom sheet. */
    private BottomSheetSwipeDetector mGestureDetector;

    /** The animator used to move the sheet to a fixed state when released by the user. */
    private ValueAnimator mSettleAnimator;

    /** The animator set responsible for swapping the bottom sheet content. */
    private AnimatorSet mContentSwapAnimatorSet;

    /** The height of the toolbar. */
    private float mToolbarHeight;

    /** The width of the view that contains the bottom sheet. */
    private float mContainerWidth;

    /** The height of the view that contains the bottom sheet. */
    private float mContainerHeight;

    /** The current state that the sheet is in. */
    @SheetState
    private int mCurrentState = SHEET_STATE_PEEK;

    /** The target sheet state. This is the state that the sheet is currently moving to. */
    @SheetState
    private int mTargetState = SHEET_STATE_NONE;

    /** Used for getting the current tab. */
    private TabModelSelector mTabModelSelector;

    /** The fullscreen manager for information about toolbar offsets. */
    private ChromeFullscreenManager mFullscreenManager;

    /** A handle to the content being shown by the sheet. */
    @Nullable
    private BottomSheetContent mSheetContent;

    /** A handle to the toolbar control container. */
    private View mControlContainer;

    /** A handle to the find-in-page toolbar. */
    private View mFindInPageView;

    /** A handle to the FrameLayout that holds the content of the bottom sheet. */
    private TouchRestrictingFrameLayout mBottomSheetContentContainer;

    /**
     * The last ratio sent to observers of onTransitionPeekToHalf(). This is used to ensure the
     * final value sent to these observers is 1.0f.
     */
    private float mLastPeekToHalfRatioSent;

    /** The FrameLayout used to hold the bottom sheet toolbar. */
    private TouchRestrictingFrameLayout mToolbarHolder;

    /**
     * The default toolbar view. This is shown when the current bottom sheet content doesn't have
     * its own toolbar and when the bottom sheet is closed.
     */
    private BottomToolbarPhone mDefaultToolbarView;

    /** Whether the {@link BottomSheet} and its children should react to touch events. */
    private boolean mIsTouchEnabled;

    /** Whether the sheet is currently open. */
    private boolean mIsSheetOpen;

    /** The activity displaying the bottom sheet. */
    private ChromeActivity mActivity;

    /** A delegate for when the action bar starts showing. */
    private ViewShiftingActionBarDelegate mActionBarDelegate;

    /** Whether or not the back button was used to enter the tab switcher. */
    private boolean mBackButtonDismissesChrome;

    /** Whether {@link #destroy()} has been called. **/
    private boolean mIsDestroyed;

    /** The token used to enable browser controls persistence. */
    private int mPersistentControlsToken;

    /** Conversion ratio of dp to px. */
    private float mDpToPx;

    /** Whether or not scroll events are currently being blocked for the 'velocity' swipe logic. */
    private boolean mVelocityLogicBlockSwipe;

    /** Whether the swipe velocity for the toolbar was recorded. */
    private boolean mIsSwipeVelocityRecorded;

    /** The speed of the swipe the last time the sheet was opened. */
    private long mLastSheetOpenMicrosPerDp;

    // TODO(twellington): Remove this after Chrome Home launches.
    /** The in-product help bubble controller used to display various help bubbles. */
    private ChromeHomeIphBubbleController mIPHBubbleController;

    /**
     * An interface defining content that can be displayed inside of the bottom sheet for Chrome
     * Home.
     */
    public interface BottomSheetContent {
        /**
         * Gets the {@link View} that holds the content to be displayed in the Chrome Home bottom
         * sheet.
         * @return The content view.
         */
        View getContentView();

        /**
         * Gets the {@link View}s that need additional padding applied to them to accommodate other
         * UI elements, such as the transparent bottom navigation menu.
         * @return The {@link View}s that need additional padding applied to them.
         */
        List<View> getViewsForPadding();

        /**
         * Get the {@link View} that contains the toolbar specific to the content being
         * displayed. If null is returned, the omnibox is used.
         *
         * @return The toolbar view.
         */
        @Nullable
        View getToolbarView();

        /**
         * @return Whether or not the toolbar is currently using a lightly colored background.
         */
        boolean isUsingLightToolbarTheme();

        /**
         * @return Whether or not the content is themed for incognito (i.e. dark colors).
         */
        boolean isIncognitoThemedContent();

        /**
         * @return The vertical scroll offset of the content view.
         */
        int getVerticalScrollOffset();

        /**
         * Called to destroy the {@link BottomSheetContent} when it is no longer in use.
         */
        void destroy();

        /**
         * @return The {@link BottomSheetContentController.ContentType} for this content.
         */
        @ContentType
        int getType();

        /**
         * @return Whether the default top padding should be applied to the content view.
         */
        boolean applyDefaultTopPadding();

        /**
         * Called to scroll to the top of {@link BottomSheetContent}.
         */
        void scrollToTop();
    }

    /**
     * Returns whether the provided bottom sheet state is in one of the stable open or closed
     * states: {@link #SHEET_STATE_FULL}, {@link #SHEET_STATE_PEEK} or {@link #SHEET_STATE_HALF}
     * @param sheetState A {@link SheetState} to test.
     */
    public static boolean isStateStable(@SheetState int sheetState) {
        switch (sheetState) {
            case SHEET_STATE_PEEK:
            case SHEET_STATE_HALF:
            case SHEET_STATE_FULL:
                return true;
            case SHEET_STATE_SCROLLING:
                return false;
            case SHEET_STATE_NONE: // Should never be tested, internal only value.
            default:
                assert false;
                return false;
        }
    }

    @Override
    public boolean shouldGestureMoveSheet(MotionEvent initialEvent, MotionEvent currentEvent) {
        // If the sheet is already open, the experiment is not enabled, or accessibility is enabled
        // there is no need to restrict the swipe area.
        if (mActivity == null || isSheetOpen() || AccessibilityUtil.isAccessibilityEnabled()) {
            return true;
        }

        boolean shouldRecordHistogram = initialEvent != currentEvent;

        if (currentEvent.getActionMasked() == MotionEvent.ACTION_DOWN) {
            mIsSwipeVelocityRecorded = false;
            mVelocityLogicBlockSwipe = false;
            shouldRecordHistogram = false;
        }

        float scrollDistanceDp = MathUtils.distance(initialEvent.getX(), initialEvent.getY(),
                                         currentEvent.getX(), currentEvent.getY())
                / mDpToPx;
        long timeDeltaMs = currentEvent.getEventTime() - initialEvent.getDownTime();
        mLastSheetOpenMicrosPerDp =
                Math.round(scrollDistanceDp > 0f ? timeDeltaMs * 1000 / scrollDistanceDp : 0f);

        String logicType = FeatureUtilities.getChromeHomeSwipeLogicType();

        // By default, the entire toolbar is swipable.
        float startX = mVisibleViewportRect.left;
        float endX = mDefaultToolbarView.getWidth() + mVisibleViewportRect.left;

        if (ChromeSwitches.CHROME_HOME_SWIPE_LOGIC_RESTRICT_AREA.equals(logicType)) {
            // Determine an area in the middle of the toolbar that is swipable. This will only
            // trigger if the expand button is disabled.
            float allowedSwipeWidth = mContainerWidth * SWIPE_ALLOWED_FRACTION;
            startX = mVisibleViewportRect.left + (mContainerWidth - allowedSwipeWidth) / 2;
            endX = startX + allowedSwipeWidth;
        } else if (ChromeSwitches.CHROME_HOME_SWIPE_LOGIC_VELOCITY.equals(logicType)
                || (ChromeFeatureList.isInitialized()
                           && ChromeFeatureList.isEnabled(
                                      ChromeFeatureList.CHROME_HOME_SWIPE_VELOCITY_FEATURE))) {
            if (mVelocityLogicBlockSwipe) return false;

            double dpPerMs = scrollDistanceDp / (double) timeDeltaMs;

            if (dpPerMs < SHEET_SWIPE_MIN_DP_PER_MS) {
                if (shouldRecordHistogram && !mIsSwipeVelocityRecorded) {
                    recordSwipeVelocity("Android.ChromeHome.OpenSheetVelocity.Fail",
                            (int) mLastSheetOpenMicrosPerDp);
                    mIsSwipeVelocityRecorded = true;
                }
                mVelocityLogicBlockSwipe = true;
                return false;
            }

            if (shouldRecordHistogram && !mIsSwipeVelocityRecorded) {
                recordSwipeVelocity("Android.ChromeHome.OpenSheetVelocity.Success",
                        (int) mLastSheetOpenMicrosPerDp);
                mIsSwipeVelocityRecorded = true;
            }
            return true;
        }

        return currentEvent.getRawX() > startX && currentEvent.getRawX() < endX;
    }

    /**
     * Record swipe velocity in microseconds per dp. This histogram will record between 0 and 20k
     * microseconds with 50 buckets.
     * @param name The name of the histogram.
     * @param microsPerDp The microseconds per dp being recorded.
     */
    private void recordSwipeVelocity(String name, int microsPerDp) {
        RecordHistogram.recordCustomCountHistogram(name, microsPerDp, 1, 60000, 50);
    }

    /**
     * Constructor for inflation from XML.
     * @param context An Android context.
     * @param atts The XML attributes.
     */
    public BottomSheet(Context context, AttributeSet atts) {
        super(context, atts);

        mMinHalfFullDistance =
                getResources().getDimensionPixelSize(R.dimen.chrome_home_min_full_half_distance);
        mToolbarShadowHeight =
                getResources().getDimensionPixelOffset(R.dimen.toolbar_shadow_height);

        mMetrics = new BottomSheetMetrics();
        addObserver(mMetrics);

        mGestureDetector = new BottomSheetSwipeDetector(context, this);
        mIsTouchEnabled = true;

        // An observer for recording metrics.
        this.addObserver(new EmptyBottomSheetObserver() {
            /**
             * Whether or not the velocity of the swipe to open the sheet should be recorded. This
             * will only be true if the sheet was opened by swipe.
             */
            private boolean mShouldRecordSwipeVelocity;

            @Override
            public void onSheetOpened(@StateChangeReason int reason) {
                mShouldRecordSwipeVelocity = reason == StateChangeReason.SWIPE;
            }

            @Override
            public void onSheetClosed(@StateChangeReason int reason) {
                boolean shouldRecordClose = reason == StateChangeReason.SWIPE
                        || reason == StateChangeReason.BACK_PRESS
                        || reason == StateChangeReason.TAP_SCRIM;
                if (mShouldRecordSwipeVelocity && shouldRecordClose) {
                    recordSwipeVelocity("Android.ChromeHome.OpenSheetVelocity.NoNavigation",
                            (int) mLastSheetOpenMicrosPerDp);
                }
                mShouldRecordSwipeVelocity = false;
            }

            @Override
            public void onLoadUrl(String url) {
                recordVelocityForNavigation();
            }

            @Override
            public void onSheetContentChanged(BottomSheetContent newContent) {
                if (newContent == null) return;
                @ContentType
                int contentId = newContent.getType();
                if (contentId != BottomSheetContentController.TYPE_SUGGESTIONS
                        && contentId != BottomSheetContentController.TYPE_INCOGNITO_HOME) {
                    recordVelocityForNavigation();
                }
            }

            /**
             * Record the velocity for the last sheet-open event.
             */
            private void recordVelocityForNavigation() {
                if (!mShouldRecordSwipeVelocity) return;
                recordSwipeVelocity("Android.ChromeHome.OpenSheetVelocity.Navigation",
                        (int) mLastSheetOpenMicrosPerDp);
                mShouldRecordSwipeVelocity = false;
            }
        });
    }

    /**
     * Called when the activity containing the {@link BottomSheet} is destroyed.
     */
    public void destroy() {
        mIsDestroyed = true;
        mIsTouchEnabled = false;
        mObservers.clear();
        endAnimations();
    }

    /**
     * Handle a back press event.
     *     - If the navigation stack is empty, the sheet will be opened to the half state.
     *         - If the tab switcher is visible, {@link ChromeActivity} will handle the event.
     *     - If the sheet is open it will be closed unless it was opened by a back press.
     * @return True if the sheet handled the back press.
     */
    public boolean handleBackPress() {
        Tab tab = getActiveTab();
        boolean consumeEvent = false;

        if (!isSheetOpen() && tab != null && !tab.canGoBack() && !isInOverviewMode()
                && tab.getLaunchType() == TabLaunchType.FROM_CHROME_UI) {
            mBackButtonDismissesChrome = true;

            setSheetState(SHEET_STATE_HALF, true);
            return true;
        } else if (isSheetOpen() && !mBackButtonDismissesChrome) {
            consumeEvent = true;
        }

        if (getSheetState() != SHEET_STATE_PEEK) {
            setSheetState(SHEET_STATE_PEEK, true, StateChangeReason.BACK_PRESS);
        }

        return consumeEvent;
    }

    /**
     * Sets whether the {@link BottomSheet} and its children should react to touch events.
     */
    public void setTouchEnabled(boolean enabled) {
        mIsTouchEnabled = enabled;
    }

    /**
     * A notification that the "expand" button for the bottom sheet has been pressed.
     */
    public void onExpandButtonPressed() {
        setSheetState(BottomSheet.SHEET_STATE_HALF, true, StateChangeReason.EXPAND_BUTTON);
    }

    /** Immediately end all animations and null the animators. */
    public void endAnimations() {
        if (mSettleAnimator != null) mSettleAnimator.end();
        mSettleAnimator = null;
        endTransitionAnimations();
    }

    /**
     * Immediately end the bottom sheet content transition animations and null the animator.
     */
    public void endTransitionAnimations() {
        if (mContentSwapAnimatorSet == null || !mContentSwapAnimatorSet.isRunning()) return;
        mContentSwapAnimatorSet.end();
        mContentSwapAnimatorSet = null;
    }

    /**
     * @return An action bar delegate that appropriately moves the sheet when the action bar is
     *         shown.
     */
    public ActionBarDelegate getActionBarDelegate() {
        return mActionBarDelegate;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent e) {
        // If touch is disabled, act like a black hole and consume touch events without doing
        // anything with them.
        if (!mIsTouchEnabled) return true;

        if (!canMoveSheet()) return false;

        return mGestureDetector.onInterceptTouchEvent(e);
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        // If touch is disabled, act like a black hole and consume touch events without doing
        // anything with them.
        if (!mIsTouchEnabled) return true;

        if (isToolbarAndroidViewHidden()) return false;

        mGestureDetector.onTouchEvent(e);

        return true;
    }

    @Override
    public boolean gatherTransparentRegion(Region region) {
        // TODO(mdjones): Figure out what this should actually be set to since the view animates
        // without necessarily calling this method again.
        region.setEmpty();
        return true;
    }

    /**
     * Defocus the omnibox.
     */
    public void defocusOmnibox() {
        if (mDefaultToolbarView == null) return;
        mDefaultToolbarView.getLocationBar().setUrlBarFocus(false);
    }

    /**
     * @param tabModelSelector A TabModelSelector for getting the current tab and activity.
     */
    public void setTabModelSelector(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
        mNtpController.setTabModelSelector(tabModelSelector);
    }

    /**
     * @param layoutManager The {@link LayoutManagerChrome} used to show and hide overview mode.
     */
    public void setLayoutManagerChrome(LayoutManagerChrome layoutManager) {
        mNtpController.setLayoutManagerChrome(layoutManager);
        getIphBubbleController().setLayoutManagerChrome(layoutManager);
    }

    /**
     * @param fullscreenManager Chrome's fullscreen manager for information about toolbar offsets.
     */
    public void setFullscreenManager(ChromeFullscreenManager fullscreenManager) {
        mFullscreenManager = fullscreenManager;
        getIphBubbleController().setFullscreenManager(fullscreenManager);
    }

    /**
     * @return Whether or not the toolbar Android View is hidden due to being scrolled off-screen.
     */
    @VisibleForTesting
    public boolean isToolbarAndroidViewHidden() {
        return mFullscreenManager == null || mFullscreenManager.getBottomControlOffset() > 0
                || mControlContainer.getVisibility() != VISIBLE;
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int heightSize = MeasureSpec.getSize(heightMeasureSpec);
        assert heightSize != 0;
        int height = heightSize + mToolbarShadowHeight;
        super.onMeasure(widthMeasureSpec, MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY));
    }

    /**
     * Adds layout change listeners to the views that the bottom sheet depends on. Namely the
     * heights of the root view and control container are important as they are used in many of the
     * calculations in this class.
     * @param root The container of the bottom sheet.
     * @param controlContainer The container for the toolbar.
     * @param activity The activity displaying the bottom sheet.
     */
    public void init(View root, View controlContainer, ChromeActivity activity) {
        mControlContainer = controlContainer;
        mToolbarHeight = mControlContainer.getHeight();
        mActivity = activity;
        mActionBarDelegate = new ViewShiftingActionBarDelegate(mActivity, this);

        getLayoutParams().height = ViewGroup.LayoutParams.MATCH_PARENT;

        mBottomSheetContentContainer =
                (TouchRestrictingFrameLayout) findViewById(R.id.bottom_sheet_content);
        mBottomSheetContentContainer.setBottomSheet(this);
        mBottomSheetContentContainer.setBackgroundColor(
                ApiCompatibilityUtils.getColor(getResources(), R.color.modern_primary_color));

        mDpToPx = mActivity.getResources().getDisplayMetrics().density;

        // Listen to height changes on the root.
        root.addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
            private int mPreviousKeyboardHeight;

            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                // Compute the new height taking the keyboard into account.
                // TODO(mdjones): Share this logic with LocationBarLayout: crbug.com/725725.
                float previousWidth = mContainerWidth;
                float previousHeight = mContainerHeight;
                mContainerWidth = right - left;
                mContainerHeight = bottom - top;

                if (previousWidth != mContainerWidth || previousHeight != mContainerHeight) {
                    updateSheetStateRatios();
                }

                int heightMinusKeyboard = (int) mContainerHeight;
                int keyboardHeight = 0;

                // Reset mVisibleViewportRect regardless of sheet open state as it is used outside
                // of calculating the keyboard height.
                mActivity.getWindow().getDecorView().getWindowVisibleDisplayFrame(
                        mVisibleViewportRect);
                if (isSheetOpen()) {
                    int decorHeight = mActivity.getWindow().getDecorView().getHeight();
                    heightMinusKeyboard = Math.min(decorHeight, mVisibleViewportRect.height());
                    keyboardHeight = (int) (mContainerHeight - heightMinusKeyboard);
                }

                if (keyboardHeight != mPreviousKeyboardHeight) {
                    // If the keyboard height changed, recompute the padding for the content area.
                    // This shrinks the content size while retaining the default background color
                    // where the keyboard is appearing. If the sheet is not showing, resize the
                    // sheet to its default state.
                    // Setting the padding is posted in a runnable for the sake of Android J.
                    // See crbug.com/751013.
                    final int finalPadding = keyboardHeight;
                    post(new Runnable() {
                        @Override
                        public void run() {
                            mBottomSheetContentContainer.setPadding(0, 0, 0, finalPadding);

                            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
                                // A layout on the toolbar holder is requested so that the toolbar
                                // doesn't disappear under certain scenarios on Android J.
                                // See crbug.com/751013.
                                mToolbarHolder.requestLayout();
                            }
                        }
                    });
                }

                if (previousHeight != mContainerHeight
                        || mPreviousKeyboardHeight != keyboardHeight) {
                    // If we are in the middle of a touch event stream (i.e. scrolling while
                    // keyboard is up) don't set the sheet state. Instead allow the gesture detector
                    // to position the sheet and make sure the keyboard hides.
                    if (mGestureDetector.isScrolling()) {
                        UiUtils.hideKeyboard(BottomSheet.this);
                    } else {
                        cancelAnimation();
                        setSheetState(mCurrentState, false);
                    }
                }

                mPreviousKeyboardHeight = keyboardHeight;
            }
        });

        // Listen to height changes on the toolbar.
        controlContainer.addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                // Make sure the size of the layout actually changed.
                if (bottom - top == oldBottom - oldTop && right - left == oldRight - oldLeft) {
                    return;
                }

                mToolbarHeight = bottom - top;
                updateSheetStateRatios();

                if (!mGestureDetector.isScrolling()) {
                    cancelAnimation();

                    // This onLayoutChange() will be called after the user enters fullscreen video
                    // mode. Ensure the sheet state is reset to peek so that the sheet does not
                    // open over the fullscreen video. See crbug.com/740499.
                    if (mFullscreenManager != null
                            && mFullscreenManager.getPersistentFullscreenMode()) {
                        setSheetState(SHEET_STATE_PEEK, false);
                    } else {
                        setSheetState(mCurrentState, false);
                    }
                }
            }
        });

        mToolbarHolder =
                (TouchRestrictingFrameLayout) mControlContainer.findViewById(R.id.toolbar_holder);
        mToolbarHolder.setBottomSheet(this);
        mDefaultToolbarView = (BottomToolbarPhone) mControlContainer.findViewById(R.id.toolbar);
        mDefaultToolbarView.setActivity(mActivity);

        mNtpController = new BottomSheetNewTabController(this, mDefaultToolbarView, mActivity);

        mActivity.getFullscreenManager().addListener(new FullscreenListener() {
            @Override
            public void onToggleOverlayVideoMode(boolean enabled) {
                if (isSheetOpen()) setSheetState(SHEET_STATE_PEEK, false);
            }

            @Override
            public void onControlsOffsetChanged(
                    float topOffset, float bottomOffset, boolean needsAnimate) {}

            @Override
            public void onContentOffsetChanged(float offset) {}

            @Override
            public void onBottomControlsHeightChanged(int bottomControlsHeight) {}
        });
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        super.onWindowFocusChanged(hasWindowFocus);

        // Trigger a relayout on window focus to correct any positioning issues when leaving Chrome
        // previously.  This is required as a layout is not triggered when coming back to Chrome
        // with the keyboard previously shown.
        if (hasWindowFocus) requestLayout();
    }

    /**
     * Set the color of the pull handle used by the toolbar.
     */
    public void updateHandleTint() {
        boolean isLightToolbarTheme = mDefaultToolbarView.isLightTheme();

        // If the current sheet content's toolbar is using a special theme, use that.
        if (mSheetContent != null && mSheetContent.getToolbarView() != null) {
            isLightToolbarTheme = mSheetContent.isUsingLightToolbarTheme();
        }

        // A light toolbar theme means the handle should be dark.
        mDefaultToolbarView.updateHandleTint(!isLightToolbarTheme);

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
            // A layout is requested so that the toolbar contents don't disappear under certain
            // scenarios on Android J. See crbug.com/769611.
            mControlContainer.requestLayout();
        }
    }

    @Override
    public int loadUrl(LoadUrlParams params, boolean incognito) {
        if (NewTabPage.isNTPUrl(params.getUrl())) {
            displayNewTabUi(incognito);
            return TabLoadStatus.PAGE_LOAD_FAILED;
        }

        // Load chrome://bookmarks, downloads, and history in the bottom sheet.
        if (handleNativePageUrl(params.getUrl())) return TabLoadStatus.PAGE_LOAD_FAILED;

        boolean isShowingNtp = isShowingNewTab();
        for (BottomSheetObserver o : mObservers) o.onLoadUrl(params.getUrl());

        // Native page URLs in this context do not need to communicate with the tab.
        if (NativePageFactory.isNativePageUrl(params.getUrl(), incognito) && !isShowingNtp) {
            return TabLoadStatus.PAGE_LOAD_FAILED;
        }

        assert mTabModelSelector != null;

        int tabLoadStatus = TabLoadStatus.DEFAULT_PAGE_LOAD;

        // First try to get the tab behind the sheet.
        if (!isShowingNtp && getActiveTab() != null && getActiveTab().isIncognito() == incognito) {
            tabLoadStatus = getActiveTab().loadUrl(params);
        } else {
            // If no compatible tab is active behind the sheet, open a new one.
            mTabModelSelector.openNewTab(
                    params, TabModel.TabLaunchType.FROM_CHROME_UI, getActiveTab(), incognito);
        }

        // In all non-native cases, minimize the sheet.
        setSheetState(SHEET_STATE_PEEK, true, StateChangeReason.NAVIGATION);

        return tabLoadStatus;
    }

    /**
     * If the URL scheme is "chrome", we try to load bookmarks, downloads, and history in the
     * bottom sheet.
     *
     * @param url The URL to be loaded.
     * @return Whether or not the URL was loaded in the sheet.
     */
    private boolean handleNativePageUrl(String url) {
        if (url == null) return false;

        Uri uri = Uri.parse(url);
        if (!UrlConstants.CHROME_SCHEME.equals(uri.getScheme())
                && !UrlConstants.CHROME_NATIVE_SCHEME.equals(uri.getScheme())) {
            return false;
        }

        if (UrlConstants.BOOKMARKS_HOST.equals(uri.getHost())) {
            mActivity.getBottomSheetContentController().showContentAndOpenSheet(
                    R.id.action_bookmarks);
        } else if (UrlConstants.DOWNLOADS_HOST.equals(uri.getHost())) {
            mActivity.getBottomSheetContentController().showContentAndOpenSheet(
                    R.id.action_downloads);
        } else if (UrlConstants.HISTORY_HOST.equals(uri.getHost())) {
            mActivity.getBottomSheetContentController().showContentAndOpenSheet(
                    R.id.action_history);
        } else {
            return false;
        }
        return true;
    }

    /**
     * Load a non-native URL in a new tab. This should be used when the new tab UI controlled by
     * {@link BottomSheetContentController} is showing.
     * @param params The params describing the URL to be loaded.
     */
    public void loadUrlInNewTab(LoadUrlParams params) {
        assert isShowingNewTab();

        loadUrl(params, mTabModelSelector.isIncognitoSelected());
    }

    @Override
    public boolean isIncognito() {
        if (getActiveTab() == null) return false;
        return getActiveTab().isIncognito();
    }

    @Override
    public int getParentId() {
        return Tab.INVALID_TAB_ID;
    }

    @Override
    public Tab getActiveTab() {
        if (mTabModelSelector == null) return null;
        if (mNtpController.isShowingNewTabUi() && isVisible()
                && getTargetSheetState() != SHEET_STATE_PEEK) {
            return null;
        }
        return mTabModelSelector.getCurrentTab();
    }

    @Override
    public boolean isVisible() {
        return mCurrentState != SHEET_STATE_PEEK;
    }

    @Override
    public boolean isContentScrolledToTop() {
        return mSheetContent == null || mSheetContent.getVerticalScrollOffset() <= 0;
    }

    @Override
    public float getCurrentOffsetPx() {
        return mContainerHeight - getTranslationY();
    }

    @Override
    public float getMinOffsetPx() {
        return getPeekRatio() * mContainerHeight;
    }

    @Override
    public float getMaxOffsetPx() {
        return getFullRatio() * mContainerHeight;
    }

    @Override
    public float getContainerHeightPx() {
        return mContainerHeight;
    }

    /**
     * @return The {@link BottomSheetNewTabController} used to present the new tab UI.
     */
    public BottomSheetNewTabController getNewTabController() {
        return mNtpController;
    }

    /**
     * Show content in the bottom sheet's content area.
     * @param content The {@link BottomSheetContent} to show, or null if no content should be shown.
     */
    public void showContent(@Nullable final BottomSheetContent content) {
        // If an animation is already running, end it.
        if (mContentSwapAnimatorSet != null) mContentSwapAnimatorSet.end();

        // If the desired content is already showing, do nothing.
        if (mSheetContent == content) return;

        List<Animator> animators = new ArrayList<>();
        mContentSwapAnimatorSet = new AnimatorSet();
        mContentSwapAnimatorSet.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                onContentSwapAnimationEnd(content);
            }
        });

        // Add an animator for the toolbar transition if needed.
        View newToolbar = content != null && content.getToolbarView() != null
                ? content.getToolbarView()
                : mDefaultToolbarView;
        View oldToolbar = mSheetContent != null && mSheetContent.getToolbarView() != null
                ? mSheetContent.getToolbarView()
                : mDefaultToolbarView;
        if (newToolbar != oldToolbar) {
            // For the toolbar transition, make sure we don't detach the default toolbar view.
            Animator transitionAnimator = getViewTransitionAnimator(
                    newToolbar, oldToolbar, mToolbarHolder, mDefaultToolbarView != oldToolbar);
            if (transitionAnimator != null) animators.add(transitionAnimator);
        }

        // Add an animator for the content transition if needed.
        View oldContent = mSheetContent != null ? mSheetContent.getContentView() : null;
        if (content == null) {
            if (oldContent != null) mBottomSheetContentContainer.removeView(oldContent);
        } else {
            View contentView = content.getContentView();
            Animator transitionAnimator = getViewTransitionAnimator(
                    contentView, oldContent, mBottomSheetContentContainer, true);
            if (transitionAnimator != null) animators.add(transitionAnimator);
        }

        // Temporarily make the background of the toolbar holder a solid color so the transition
        // doesn't appear to show a hole in the toolbar.
        int colorId = content == null || !content.isIncognitoThemedContent()
                ? R.color.modern_primary_color
                : R.color.incognito_primary_color;
        if (!mIsSheetOpen || (content != null && content.isIncognitoThemedContent())
                || (mSheetContent != null && mSheetContent.isIncognitoThemedContent())) {
            // If the sheet is closed, the bottom sheet content container is invisible, so
            // background color is needed on the toolbar holder to prevent a blank rectangle from
            // appearing during the content transition.
            mToolbarHolder.setBackgroundColor(
                    ApiCompatibilityUtils.getColor(getResources(), colorId));
        }
        mBottomSheetContentContainer.setBackgroundColor(
                ApiCompatibilityUtils.getColor(getResources(), colorId));

        // Set color on the content view to compensate for a JellyBean bug (crbug.com/766237).
        if (content != null) {
            content.getContentView().setBackgroundColor(
                    ApiCompatibilityUtils.getColor(getResources(), colorId));
        }

        // Return early if there are no animators to run.
        if (animators.isEmpty()) {
            onContentSwapAnimationEnd(content);
            return;
        }

        mContentSwapAnimatorSet.playTogether(animators);
        mContentSwapAnimatorSet.start();

        // If the existing content is null or the tab switcher assets are showing, end the animation
        // immediately.
        if (mSheetContent == null || mDefaultToolbarView.isInTabSwitcherMode()
                || SysUtils.isLowEndDevice()) {
            mContentSwapAnimatorSet.end();
        }
    }

    /**
     * Called when the animation to swap BottomSheetContent ends.
     * @param content The BottomSheetContent showing at the end of the animation.
     */
    private void onContentSwapAnimationEnd(BottomSheetContent content) {
        if (mIsDestroyed) return;

        onSheetContentChanged(content);
        mContentSwapAnimatorSet = null;
    }

    /**
     * Creates a transition animation between two views. The old view is faded out completely
     * before the new view is faded in. There is an option to detach the old view or not.
     * @param newView The new view to transition to.
     * @param oldView The old view to transition from.
     * @param parent The parent for newView and oldView.
     * @param detachOldView Whether or not to detach the old view once faded out.
     * @return An animator that runs the specified animation or null if no animation should be run.
     */
    @Nullable
    private Animator getViewTransitionAnimator(final View newView, final View oldView,
            final ViewGroup parent, final boolean detachOldView) {
        if (newView == oldView) return null;

        AnimatorSet animatorSet = new AnimatorSet();
        List<Animator> animators = new ArrayList<>();

        newView.setVisibility(View.VISIBLE);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                && !ValueAnimator.areAnimatorsEnabled()) {
            if (oldView != null) {
                // Post a runnable to remove the old view to prevent issues related to the keyboard
                // showing while swapping contents. See https://crbug.com/799252.
                post(() -> { swapViews(newView, oldView, parent, detachOldView); });
            } else {
                if (parent != newView.getParent()) parent.addView(newView);
            }

            newView.setAlpha(1);

            return null;
        }

        // Fade out the old view.
        if (oldView != null) {
            ValueAnimator fadeOutAnimator = ObjectAnimator.ofFloat(oldView, View.ALPHA, 0);
            fadeOutAnimator.setDuration(TRANSITION_DURATION_MS);
            fadeOutAnimator.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationEnd(Animator animation) {
                    swapViews(newView, oldView, parent, detachOldView);
                }
            });
            animators.add(fadeOutAnimator);
        } else {
            // Normally the new view is added at the end of the fade-out animation of the old view,
            // if there is no old view, attach the new one immediately.
            if (parent != newView.getParent()) parent.addView(newView);
        }

        // Fade in the new view.
        newView.setAlpha(0);
        ValueAnimator fadeInAnimator = ObjectAnimator.ofFloat(newView, View.ALPHA, 1);
        fadeInAnimator.setDuration(TRANSITION_DURATION_MS);
        animators.add(fadeInAnimator);

        animatorSet.playSequentially(animators);

        return animatorSet;
    }

    /**
     * Removes the oldView (or sets it to invisible) and adds the new view to the specified parent.
     * @param newView The new view to transition to.
     * @param oldView The old view to transition from.
     * @param parent The parent for newView and oldView.
     * @param detachOldView Whether or not to detach the old view once faded out.
     */
    private void swapViews(final View newView, final View oldView, final ViewGroup parent,
            final boolean detachOldView) {
        if (detachOldView && oldView.getParent() != null) {
            parent.removeView(oldView);
        } else {
            oldView.setVisibility(View.INVISIBLE);
        }
        if (parent != newView.getParent()) parent.addView(newView);
    }

    /**
     * A notification that the sheet is exiting the peek state into one that shows content.
     * @param reason The reason the sheet was opened, if any.
     */
    private void onSheetOpened(@StateChangeReason int reason) {
        if (mIsSheetOpen) return;

        mIsSheetOpen = true;

        // Make sure the toolbar is visible before expanding the sheet.
        Tab tab = getActiveTab();
        if (isToolbarAndroidViewHidden() && tab != null) {
            tab.updateBrowserControlsState(BrowserControlsState.SHOWN, false);
        }

        mBottomSheetContentContainer.setVisibility(View.VISIBLE);

        // Browser controls should stay visible until the sheet is closed.
        mPersistentControlsToken =
                mFullscreenManager.getBrowserVisibilityDelegate().showControlsPersistent();

        dismissSelectedText();
        for (BottomSheetObserver o : mObservers) o.onSheetOpened(reason);
        mActivity.addViewObscuringAllTabs(this);
    }

    /**
     * A notification that the sheet has returned to the peeking state.
     * @param reason The {@link StateChangeReason} that the sheet was closed, if any.
     */
    private void onSheetClosed(@StateChangeReason int reason) {
        if (!mIsSheetOpen) return;
        mBottomSheetContentContainer.setVisibility(View.INVISIBLE);
        mBackButtonDismissesChrome = false;
        mIsSheetOpen = false;

        // Update the browser controls since they are permanently shown while the sheet is open.
        mFullscreenManager.getBrowserVisibilityDelegate().hideControlsPersistent(
                mPersistentControlsToken);

        for (BottomSheetObserver o : mObservers) o.onSheetClosed(reason);
        announceForAccessibility(getResources().getString(R.string.bottom_sheet_closed));
        clearFocus();
        mActivity.removeViewObscuringAllTabs(this);

        setFocusable(false);
        setFocusableInTouchMode(false);
        setContentDescription(null);
    }

    /**
     * Updates the bottom sheet's state ratios and adjusts the sheet's state if necessary.
     */
    private void updateSheetStateRatios() {
        if (mContainerHeight <= 0) return;

        // Though mStateRatios is a static constant, the peeking ratio is computed here because
        // the correct toolbar height and container height are not know until those views are
        // inflated. The other views are a specific DP distance from the top and bottom and are
        // also updated.
        mStateRatios[0] = mToolbarHeight / mContainerHeight;
        mStateRatios[1] = HALF_HEIGHT_RATIO;
        // The max height ratio will be greater than 1 to account for the toolbar shadow.
        mStateRatios[2] = (mContainerHeight + mToolbarShadowHeight) / mContainerHeight;

        if (mCurrentState == SHEET_STATE_HALF && isSmallScreen()) {
            setSheetState(SHEET_STATE_FULL, false);
        }
    }

    /**
     * Cancels and nulls the height animation if it exists.
     */
    private void cancelAnimation() {
        if (mSettleAnimator == null) return;
        mSettleAnimator.cancel();
        mSettleAnimator = null;
    }

    /**
     * Creates the sheet's animation to a target state.
     * @param targetState The target state.
     * @param reason The reason the sheet started animation.
     */
    private void createSettleAnimation(
            @SheetState final int targetState, @StateChangeReason final int reason) {
        mTargetState = targetState;
        mSettleAnimator =
                ValueAnimator.ofFloat(getCurrentOffsetPx(), getSheetHeightForState(targetState));
        mSettleAnimator.setDuration(BASE_ANIMATION_DURATION_MS);
        mSettleAnimator.setInterpolator(mInterpolator);

        // When the animation is canceled or ends, reset the handle to null.
        mSettleAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animator) {
                if (mIsDestroyed) return;

                mSettleAnimator = null;
                setInternalCurrentState(targetState, reason);
                mTargetState = SHEET_STATE_NONE;
            }
        });

        mSettleAnimator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator animator) {
                setSheetOffsetFromBottom((Float) animator.getAnimatedValue());
            }
        });

        setInternalCurrentState(SHEET_STATE_SCROLLING, reason);
        mSettleAnimator.start();
    }

    /**
     * Sets the sheet's offset relative to the bottom of the screen.
     * @param offset The offset that the sheet should be.
     */
    private void setSheetOffsetFromBottom(float offset) {
        if (MathUtils.areFloatsEqual(offset, getCurrentOffsetPx())) return;

        setTranslationY(mContainerHeight - offset);
        sendOffsetChangeEvents();
    }

    @Override
    public void setSheetOffset(float offset, boolean shouldAnimate) {
        cancelAnimation();

        if (shouldAnimate) {
            float velocityY = getCurrentOffsetPx() - offset;

            @BottomSheet.SheetState
            int targetState = getTargetSheetState(offset, -velocityY);

            setSheetState(targetState, true, BottomSheet.StateChangeReason.SWIPE);

            for (BottomSheetObserver o : mObservers) o.onSheetReleased();
        } else {
            setInternalCurrentState(
                    BottomSheet.SHEET_STATE_SCROLLING, BottomSheet.StateChangeReason.SWIPE);
            setSheetOffsetFromBottom(offset);
        }
    }

    /**
     * Deselects any text in the active tab's web contents and dismisses the text controls.
     */
    private void dismissSelectedText() {
        Tab activeTab = getActiveTab();
        if (activeTab == null) return;

        WebContents webContents = activeTab.getWebContents();
        if (webContents == null) return;
        SelectionPopupController.fromWebContents(webContents).clearSelection();
    }

    /**
     * This is the same as {@link #setSheetOffsetFromBottom(float)} but exclusively for testing.
     * @param offset The offset to set the sheet to.
     */
    @VisibleForTesting
    public void setSheetOffsetFromBottomForTesting(float offset) {
        setSheetOffsetFromBottom(offset);
    }

    /**
     * @return The ratio of the height of the screen that the peeking state is.
     */
    @VisibleForTesting
    public float getPeekRatio() {
        return mStateRatios[0];
    }

    /**
     * @return The ratio of the height of the screen that the half expanded state is.
     */
    @VisibleForTesting
    public float getHalfRatio() {
        return mStateRatios[1];
    }

    /**
     * @return The ratio of the height of the screen that the fully expanded state is.
     */
    @VisibleForTesting
    public float getFullRatio() {
        return mStateRatios[2];
    }

    /**
     * @return The height of the container that the bottom sheet exists in.
     */
    @VisibleForTesting
    public float getSheetContainerHeight() {
        return mContainerHeight;
    }

    /**
     * Sends notifications if the sheet is transitioning from the peeking to half expanded state and
     * from the peeking to fully expanded state. The peek to half events are only sent when the
     * sheet is between the peeking and half states.
     */
    private void sendOffsetChangeEvents() {
        float screenRatio = mContainerHeight > 0 ? getCurrentOffsetPx() / mContainerHeight : 0;

        // This ratio is relative to the peek and full positions of the sheet.
        float peekFullRatio = MathUtils.clamp(
                (screenRatio - getPeekRatio()) / (getFullRatio() - getPeekRatio()), 0, 1);

        for (BottomSheetObserver o : mObservers) {
            o.onSheetOffsetChanged(MathUtils.areFloatsEqual(peekFullRatio, 0) ? 0 : peekFullRatio);
        }

        // This ratio is relative to the peek and half positions of the sheet.
        float peekHalfRatio = MathUtils.clamp(
                (screenRatio - getPeekRatio()) / (getHalfRatio() - getPeekRatio()), 0, 1);

        // If the ratio is close enough to zero, just set it to zero.
        if (MathUtils.areFloatsEqual(peekHalfRatio, 0f)) peekHalfRatio = 0f;

        if (mLastPeekToHalfRatioSent < 1f || peekHalfRatio < 1f) {
            mLastPeekToHalfRatioSent = peekHalfRatio;
            for (BottomSheetObserver o : mObservers) {
                o.onTransitionPeekToHalf(peekHalfRatio);
            }
        }
    }

    /**
     * @see #setSheetState(int, boolean, int)
     */
    public void setSheetState(@SheetState int state, boolean animate) {
        setSheetState(state, animate, StateChangeReason.NONE);
    }

    /**
     * Moves the sheet to the provided state.
     * @param state The state to move the panel to. This cannot be SHEET_STATE_SCROLLING or
     *              SHEET_STATE_NONE.
     * @param animate If true, the sheet will animate to the provided state, otherwise it will
     *                move there instantly.
     * @param reason The reason the sheet state is changing. This can be specified to indicate to
     *               observers that a more specific event has occurred, otherwise
     *               STATE_CHANGE_REASON_NONE can be used.
     */
    public void setSheetState(
            @SheetState int state, boolean animate, @StateChangeReason int reason) {
        assert state != SHEET_STATE_SCROLLING && state != SHEET_STATE_NONE;

        // Half state is not valid on small screens.
        if (state == SHEET_STATE_HALF && isSmallScreen()) state = SHEET_STATE_FULL;

        mTargetState = state;

        cancelAnimation();

        if (animate && state != mCurrentState) {
            createSettleAnimation(state, reason);
        } else {
            setSheetOffsetFromBottom(getSheetHeightForState(state));
            setInternalCurrentState(mTargetState, reason);
            mTargetState = SHEET_STATE_NONE;
        }
    }

    /**
     * @return The target state that the sheet is moving to during animation. If the sheet is
     *         stationary or a target state has not been determined, SHEET_STATE_NONE will be
     *         returned. A target state will be set when the user releases the sheet from drag
     *         ({@link BottomSheetObserver#onSheetReleased()}) and has begun animation to the next
     *         state.
     */
    public int getTargetSheetState() {
        return mTargetState;
    }

    /**
     * @return The current state of the bottom sheet. If the sheet is animating, this will be the
     *         state the sheet is animating to.
     */
    @SheetState
    public int getSheetState() {
        return mCurrentState;
    }

    /** @return Whether the sheet is currently open. */
    public boolean isSheetOpen() {
        return mIsSheetOpen;
    }

    /**
     * Set the current state of the bottom sheet. This is for internal use to notify observers of
     * state change events.
     * @param state The current state of the sheet.
     * @param reason The reason the state is changing if any.
     */
    private void setInternalCurrentState(@SheetState int state, @StateChangeReason int reason) {
        if (state == mCurrentState) return;

        // TODO(mdjones): This shouldn't be able to happen, but does occasionally during layout.
        //                Fix the race condition that is making this happen.
        if (state == SHEET_STATE_NONE) {
            setSheetState(getTargetSheetState(getCurrentOffsetPx(), 0), false);
            return;
        }

        mCurrentState = state;

        if (mCurrentState == SHEET_STATE_HALF || mCurrentState == SHEET_STATE_FULL) {
            announceForAccessibility(mCurrentState == SHEET_STATE_FULL
                            ? getResources().getString(R.string.bottom_sheet_opened_full)
                            : getResources().getString(R.string.bottom_sheet_opened_half));

            // TalkBack will announce the content description if it has changed, so wait to set the
            // content description until after announcing full/half height.
            setFocusable(true);
            setFocusableInTouchMode(true);
            setContentDescription(
                    getResources().getString(R.string.bottom_sheet_accessibility_description));
            if (getFocusedChild() == null) requestFocus();
        }

        for (BottomSheetObserver o : mObservers) {
            o.onSheetStateChanged(mCurrentState);
        }

        if (state == SHEET_STATE_PEEK) {
            onSheetClosed(reason);
        } else {
            onSheetOpened(reason);
        }
    }

    /**
     * If the animation to settle the sheet in one of its states is running.
     * @return True if the animation is running.
     */
    public boolean isRunningSettleAnimation() {
        return mSettleAnimator != null;
    }

    /**
     * @return Whether a content swap animation is in progress.
     */
    public boolean isRunningContentSwapAnimation() {
        return mContentSwapAnimatorSet != null && mContentSwapAnimatorSet.isRunning();
    }

    /**
     * @return The current sheet content, or null if there is no content.
     */
    @VisibleForTesting
    public @Nullable BottomSheetContent getCurrentSheetContent() {
        return mSheetContent;
    }

    /**
     * @return The {@link BottomSheetMetrics} used to record user actions and histograms.
     */
    public BottomSheetMetrics getBottomSheetMetrics() {
        return mMetrics;
    }

    /**
     * Gets the height of the bottom sheet based on a provided state.
     * @param state The state to get the height from.
     * @return The height of the sheet at the provided state.
     */
    public float getSheetHeightForState(@SheetState int state) {
        return mStateRatios[state] * mContainerHeight;
    }

    /**
     * Adds an observer to the bottom sheet.
     * @param observer The observer to add.
     */
    public void addObserver(BottomSheetObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes an observer to the bottom sheet.
     * @param observer The observer to remove.
     */
    public void removeObserver(BottomSheetObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Gets the target state of the sheet based on the sheet's height and velocity.
     * @param sheetHeight The current height of the sheet.
     * @param yVelocity The current Y velocity of the sheet. This is only used for determining the
     *                  scroll or fling direction. If this value is positive, the movement is from
     *                  bottom to top.
     * @return The target state of the bottom sheet.
     */
    @SheetState
    private int getTargetSheetState(float sheetHeight, float yVelocity) {
        if (sheetHeight <= getMinOffsetPx()) return SHEET_STATE_PEEK;
        if (sheetHeight >= getMaxOffsetPx()) return SHEET_STATE_FULL;

        boolean isMovingDownward = yVelocity < 0;
        boolean shouldSkipHalfState = isMovingDownward || isSmallScreen();

        // First, find the two states that the sheet height is between.
        @SheetState
        int nextState = sStates[0];

        @SheetState
        int prevState = nextState;
        for (int i = 0; i < sStates.length; i++) {
            if (sStates[i] == SHEET_STATE_HALF && shouldSkipHalfState) continue;
            prevState = nextState;
            nextState = sStates[i];
            // The values in PanelState are ascending, they should be kept that way in order for
            // this to work.
            if (sheetHeight >= getSheetHeightForState(prevState)
                    && sheetHeight < getSheetHeightForState(nextState)) {
                break;
            }
        }

        // If the desired height is close enough to a certain state, depending on the direction of
        // the velocity, move to that state.
        float lowerBound = getSheetHeightForState(prevState);
        float distance = getSheetHeightForState(nextState) - lowerBound;

        float threshold =
                shouldSkipHalfState ? THRESHOLD_TO_NEXT_STATE_2 : THRESHOLD_TO_NEXT_STATE_3;
        float thresholdToNextState = yVelocity < 0.0f ? 1.0f - threshold : threshold;

        if ((sheetHeight - lowerBound) / distance > thresholdToNextState) {
            return nextState;
        }
        return prevState;
    }

    public boolean isSmallScreen() {
        // A small screen is defined by there being less than 160dp between half and full states.
        float fullToHalfDiff = (getFullRatio() - getHalfRatio()) * mContainerHeight;
        return fullToHalfDiff < mMinHalfFullDistance;
    }

    @Override
    public void onFadingViewClick() {
        setSheetState(SHEET_STATE_PEEK, true, StateChangeReason.TAP_SCRIM);
    }

    @Override
    public void onFadingViewVisibilityChanged(boolean visible) {}

    /**
     * @return Whether the {@link BottomSheetNewTabController} is showing the new tab UI. This
     *         returns true if a normal or incognito new tab is showing.
     */
    public boolean isShowingNewTab() {
        return mNtpController.isShowingNewTabUi();
    }

    /**
     * Tells {@link BottomSheetNewTabController} to display the new tab UI.
     * @param isIncognito Whether to display the incognito new tab UI.
     */
    public void displayNewTabUi(boolean isIncognito) {
        mNtpController.displayNewTabUi(isIncognito);
    }

    /**
     * Tells {@link BottomSheetNewTabController} to display the specified content in a new tab.
     * @param isIncognito Whether to display the incognito new tab UI.
     * @param actionId The action id of the bottom sheet content to be displayed.
     */
    public void displayNewTabUi(boolean isIncognito, int actionId) {
        mNtpController.displayNewTabUi(isIncognito, actionId);
    }

    /**
     * Show the in-product help bubble for the {@link BottomSheet} if it has not already been shown.
     * This method must be called after the toolbar has had at least one layout pass.
     */
    public void showColdStartHelpBubble() {
        getIphBubbleController().showColdStartHelpBubble();
    }

    /**
     * Show the in-product help bubble for the {@link BottomSheet} if conditions are right. This
     * method must be called after the toolbar has had at least one layout pass and
     * ChromeFeatureList has been initialized.
     * @param fromMenu Whether the help bubble is being displayed in response to a click on the
     *                 IPH menu header.
     * @param fromPullToRefresh Whether the help bubble is being displayed due to a pull to refresh.
     */
    public void maybeShowHelpBubble(boolean fromMenu, boolean fromPullToRefresh) {
        getIphBubbleController().maybeShowHelpBubble(fromMenu, fromPullToRefresh);
    }

    /** Gets the IPH bubble controller, creating it if necessary. */
    private ChromeHomeIphBubbleController getIphBubbleController() {
        if (mIPHBubbleController == null) {
            mIPHBubbleController = new ChromeHomeIphBubbleController(
                    getContext(), mDefaultToolbarView, mControlContainer, this);
        }

        return mIPHBubbleController;
    }

    /**
     * @return The default toolbar view.
     */
    @VisibleForTesting
    public @Nullable View getDefaultToolbarView() {
        return mDefaultToolbarView;
    }

    /**
     * @return The height of the toolbar holder.
     */
    public int getToolbarContainerHeight() {
        return mToolbarHolder != null ? mToolbarHolder.getHeight() : 0;
    }

    /**
     * @return The height of the bottom navigation menu. Returns 0 if the {@link ChromeActivity} or
     * {@link BottomSheetContentController} are null.
     */
    public int getBottomNavHeight() {
        BottomSheetContentController contentController =
                mActivity != null ? mActivity.getBottomSheetContentController() : null;
        return contentController != null ? contentController.getBottomNavHeight() : 0;
    }

    /**
     * @return The height of the toolbar shadow.
     */
    public int getToolbarShadowHeight() {
        return mToolbarShadowHeight;
    }

    /**
     * @return Whether or not the bottom sheet's toolbar is using the expand button.
     */
    public boolean isUsingExpandButton() {
        return mDefaultToolbarView.isUsingExpandButton();
    }

    /**
     * @return Whether or not the browser is in overview mode.
     */
    private boolean isInOverviewMode() {
        return mActivity != null && mActivity.isInOverviewMode();
    }

    /**
     * Checks whether the sheet can be moved. It cannot be moved when the activity is in overview
     * mode, when "find in page" is visible, or when the toolbar is hidden.
     */
    private boolean canMoveSheet() {
        if (mFindInPageView == null) mFindInPageView = findViewById(R.id.find_toolbar);
        boolean isFindInPageVisible =
                mFindInPageView != null && mFindInPageView.getVisibility() == View.VISIBLE;

        return !isToolbarAndroidViewHidden()
                && (!isInOverviewMode() || mNtpController.isShowingNewTabUi())
                && !isFindInPageVisible;
    }

    /**
     * Called when the sheet content has changed, to update dependent state and notify observers.
     * @param content The new sheet content, or null if the sheet has no content.
     */
    private void onSheetContentChanged(@Nullable final BottomSheetContent content) {
        mSheetContent = content;
        for (BottomSheetObserver o : mObservers) {
            o.onSheetContentChanged(content);
        }
        updateHandleTint();
        mToolbarHolder.setBackgroundColor(Color.TRANSPARENT);
    }

    /**
     * @return The bottom sheet's help bubble if it exists.
     */
    @VisibleForTesting
    public @Nullable TextBubble getHelpBubbleForTests() {
        return getIphBubbleController().getHelpBubbleForTests();
    }
}

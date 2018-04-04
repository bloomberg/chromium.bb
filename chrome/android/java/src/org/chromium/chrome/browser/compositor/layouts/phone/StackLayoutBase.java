// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import android.content.Context;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.SystemClock;
import android.support.annotation.IntDef;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.animation.Interpolator;
import android.widget.FrameLayout;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.ChromeAnimation.Animatable;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EventFilter;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.GestureEventFilter;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.GestureHandler;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.ScrollDirection;
import org.chromium.chrome.browser.compositor.layouts.phone.stack.Stack;
import org.chromium.chrome.browser.compositor.layouts.phone.stack.StackTab;
import org.chromium.chrome.browser.compositor.scene_layer.SceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.TabListSceneLayer;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.partnercustomizations.HomepageManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.interpolators.BakedBezierInterpolator;
import org.chromium.ui.resources.ResourceManager;

import java.io.Serializable;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;

/**
 * Base class for layouts that show one or more stacks of tabs.
 */
public abstract class StackLayoutBase
        extends Layout implements Animatable<StackLayoutBase.Property> {
    public enum Property {
        INNER_MARGIN_PERCENT,
        STACK_SNAP,
        STACK_OFFSET_Y_PERCENT,
    }

    @IntDef({DRAG_DIRECTION_NONE, DRAG_DIRECTION_HORIZONTAL, DRAG_DIRECTION_VERTICAL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DragDirection {}
    private static final int DRAG_DIRECTION_NONE = 0;
    private static final int DRAG_DIRECTION_HORIZONTAL = 1;
    private static final int DRAG_DIRECTION_VERTICAL = 2;

    @IntDef({SWIPE_MODE_NONE, SWIPE_MODE_SEND_TO_STACK, SWIPE_MODE_SWITCH_STACK})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SwipeMode {}
    protected static final int SWIPE_MODE_NONE = 0;
    protected static final int SWIPE_MODE_SEND_TO_STACK = 1;
    protected static final int SWIPE_MODE_SWITCH_STACK = 2;

    protected static final int INVALID_STACK_INDEX = -1;

    private static final String TAG = "StackLayoutBase";
    // Width of the partially shown stack when there are multiple stacks.
    private static final int MIN_INNER_MARGIN_PERCENT_DP = 55;
    private static final float INNER_MARGIN_PERCENT_PERCENT = 0.17f;

    // Speed of the automatic fling in dp/ms
    private static final float FLING_SPEED_DP = 1.5f; // dp / ms
    private static final int FLING_MIN_DURATION = 100; // ms

    private static final float THRESHOLD_TO_SWITCH_STACK = 0.4f;
    private static final int NEW_TAB_ANIMATION_DURATION_MS = 300;

    public static final int MODERN_TOP_MARGIN_DP = 16;

    /**
     * The delta time applied on the velocity from the fling. This is to compute the kick to help
     * switching the stack.
     */
    private static final float SWITCH_STACK_FLING_DT = 1.0f / 30.0f;

    /** The list of potentially visible stacks. */
    protected final ArrayList<Stack> mStacks;

    /** Rectangles that defines the area where each stack need to be laid out. */
    protected final ArrayList<RectF> mStackRects;

    private int mStackAnimationCount;

    private float mFlingSpeed; // pixel/ms

    private boolean mClicked;

    // If not overscroll, then mRenderedScrollIndex == mScrollIndex;
    // Otherwise, mRenderedScrollIndex is updated with the actual index passed in
    // from the event handler; and mRenderedScrollIndex is the value we get
    // after map mScrollIndex through a decelerate function.
    // Here we use float as index so we can smoothly animate the transition between stack.
    private float mRenderedScrollOffset;
    private float mScrollIndexOffset;

    private final int mMinMaxInnerMargin;
    private float mInnerMarginPercent;
    private float mStackOffsetYPercent;

    @DragDirection
    private int mDragDirection = DRAG_DIRECTION_NONE;

    @SwipeMode
    private int mInputMode = SWIPE_MODE_NONE;
    private float mLastOnDownX;
    private float mLastOnDownY;
    private long mLastOnDownTimeStamp;

    private float mWidth;
    private float mHeight;
    private int mOrientation;

    // Pre-allocated temporary arrays that store id of visible tabs.
    // They can be used to call populatePriorityVisibilityList.
    // We use StackTab[] instead of ArrayList<StackTab> because the sorting function does
    // an allocation to iterate over the elements.
    // Do not use out of the context of {@link #updateTabPriority}.
    private StackTab[] mSortedPriorityArray;

    private final ArrayList<Integer> mVisibilityArray = new ArrayList<Integer>();
    private final VisibilityComparator mVisibilityComparator = new VisibilityComparator();
    private final OrderComparator mOrderComparator = new OrderComparator();
    private Comparator<StackTab> mSortingComparator = mVisibilityComparator;

    private static final int LAYOUTTAB_ASYNCHRONOUS_INITIALIZATION_BATCH_SIZE = 4;
    private boolean mDelayedLayoutTabInitRequired;

    /**
     * Temporarily stores the index of the selected tab stack. This is used to set the currently
     * selected stack in TabModelSelector once the stack-switching animation finishes.
     */
    protected int mTemporarySelectedStack = INVALID_STACK_INDEX;

    // Orientation Variables
    private PortraitViewport mCachedPortraitViewport;
    private PortraitViewport mCachedLandscapeViewport;

    private final ViewGroup mViewContainer;

    private final GestureEventFilter mGestureEventFilter;
    private final TabListSceneLayer mSceneLayer;

    private StackLayoutGestureHandler mGestureHandler;

    /** A {@link LayoutTab} used for new tab animations. */
    private LayoutTab mNewTabLayoutTab;

    /**
     * Whether or not the new layout tab has been properly initialized (a frame can occur between
     * creation and initialization).
     */
    private boolean mIsNewTabInitialized;

    private class StackLayoutGestureHandler implements GestureHandler {
        @Override
        public void onDown(float x, float y, boolean fromMouse, int buttons) {
            long time = time();
            mDragDirection = DRAG_DIRECTION_NONE;
            mLastOnDownX = x;
            mLastOnDownY = y;
            mLastOnDownTimeStamp = time;
            mStacks.get(getTabStackIndex()).onDown(time);
        }

        @Override
        public void onUpOrCancel() {
            onUpOrCancel(time());
        }

        @Override
        public void drag(float x, float y, float dx, float dy, float tx, float ty) {
            @SwipeMode
            int oldInputMode = mInputMode;
            long time = time();
            float amountX = dx;
            float amountY = dy;
            mInputMode = computeInputMode(time, x, y, amountX, amountY);

            if (mDragDirection == DRAG_DIRECTION_HORIZONTAL) amountY = 0;
            if (mDragDirection == DRAG_DIRECTION_VERTICAL) amountX = 0;

            if (oldInputMode == SWIPE_MODE_SEND_TO_STACK && mInputMode == SWIPE_MODE_SWITCH_STACK) {
                mStacks.get(getTabStackIndex()).onUpOrCancel(time);
            } else if (oldInputMode == SWIPE_MODE_SWITCH_STACK
                    && mInputMode == SWIPE_MODE_SEND_TO_STACK) {
                onUpOrCancel(time);
            }

            if (mInputMode == SWIPE_MODE_SEND_TO_STACK) {
                mStacks.get(getTabStackIndex()).drag(time, x, y, amountX, amountY);
            } else if (mInputMode == SWIPE_MODE_SWITCH_STACK) {
                scrollStacks(isUsingHorizontalLayout() ? amountY : amountX);
            }
        }

        @Override
        public void click(float x, float y, boolean fromMouse, int buttons) {
            // Click event happens before the up event. mClicked is set to mute the up event.
            mClicked = true;
            PortraitViewport viewportParams = getViewportParameters();
            final int stackIndexDeltaAt = viewportParams.getStackIndexDeltaAt(x, y);
            if (stackIndexDeltaAt == 0) {
                mStacks.get(getTabStackIndex()).click(time(), x, y);
            } else {
                final int newStackIndex = getTabStackIndex() + stackIndexDeltaAt;
                if (newStackIndex < 0 || newStackIndex >= mStacks.size()) return;
                if (!mStacks.get(newStackIndex).isDisplayable()) return;
                flingStacks(newStackIndex);
            }
            requestStackUpdate();
        }

        @Override
        public void fling(float x, float y, float velocityX, float velocityY) {
            long time = time();
            float vx = velocityX;
            float vy = velocityY;

            if (mInputMode == SWIPE_MODE_NONE) {
                mInputMode = computeInputMode(
                        time, x, y, vx * SWITCH_STACK_FLING_DT, vy * SWITCH_STACK_FLING_DT);
            }

            if (mInputMode == SWIPE_MODE_SEND_TO_STACK) {
                mStacks.get(getTabStackIndex()).fling(time, x, y, vx, vy);
            } else if (mInputMode == SWIPE_MODE_SWITCH_STACK) {
                final float velocity = isUsingHorizontalLayout() ? vy : vx;
                final float origin = isUsingHorizontalLayout() ? y : x;
                final float max = isUsingHorizontalLayout() ? getHeight() : getWidth();
                final float predicted = origin + velocity * SWITCH_STACK_FLING_DT;
                final float delta = MathUtils.clamp(predicted, 0, max) - origin;
                scrollStacks(delta);
            }
            requestStackUpdate();
        }

        @Override
        public void onLongPress(float x, float y) {
            mStacks.get(getTabStackIndex()).onLongPress(time(), x, y);
        }

        @Override
        public void onPinch(float x0, float y0, float x1, float y1, boolean firstEvent) {
            mStacks.get(getTabStackIndex()).onPinch(time(), x0, y0, x1, y1, firstEvent);
        }

        private void onUpOrCancel(long time) {
            int currentIndex = getTabStackIndex();
            if (!mClicked
                    && Math.abs(currentIndex + mRenderedScrollOffset) > THRESHOLD_TO_SWITCH_STACK) {
                int nextIndex;
                if (currentIndex + mRenderedScrollOffset < 0) {
                    nextIndex = currentIndex + 1;
                } else {
                    nextIndex = currentIndex - 1;
                }
                if (mStacks.get(nextIndex).isDisplayable()) {
                    setActiveStackState(nextIndex);
                }
            }

            mClicked = false;
            finishScrollStacks();
            mStacks.get(getTabStackIndex()).onUpOrCancel(time);
            mInputMode = SWIPE_MODE_NONE;
        }

        private long time() {
            return LayoutManager.time();
        }
    }

    /**
     * @param context     The current Android's context.
     * @param updateHost  The {@link LayoutUpdateHost} view for this layout.
     * @param renderHost  The {@link LayoutRenderHost} view for this layout.
     */
    public StackLayoutBase(
            Context context, LayoutUpdateHost updateHost, LayoutRenderHost renderHost) {
        super(context, updateHost, renderHost);

        mGestureHandler = new StackLayoutGestureHandler();
        mGestureEventFilter = new GestureEventFilter(context, mGestureHandler);

        mMinMaxInnerMargin = (int) (MIN_INNER_MARGIN_PERCENT_DP + 0.5);
        mFlingSpeed = FLING_SPEED_DP;

        mStacks = new ArrayList<Stack>();
        mStackRects = new ArrayList<RectF>();
        mViewContainer = new FrameLayout(getContext());
        mSceneLayer = new TabListSceneLayer();
    }

    /**
     * Whether or not we're currently having the tabs scroll horizontally (as opposed to
     * vertically).
     */
    private boolean isUsingHorizontalLayout() {
        return getOrientation() == Orientation.LANDSCAPE
                || ChromeFeatureList.isEnabled(ChromeFeatureList.HORIZONTAL_TAB_SWITCHER_ANDROID);
    }

    /**
     * Updates this layout to show one tab stack for each of the passed-in TabLists. Takes a
     * reference to the lists param and expects it not to change.
     * @param lists The list of TabLists to use.
     */
    protected void setTabLists(List<TabList> lists) {
        if (mStacks.size() > lists.size()) {
            mStacks.subList(lists.size(), mStacks.size()).clear();
        }
        while (mStacks.size() < lists.size()) {
            Stack stack = new Stack(getContext(), this);
            stack.notifySizeChanged(mWidth, mHeight, mOrientation);
            mStacks.add(stack);
        }

        for (int i = 0; i < lists.size(); i++) {
            mStacks.get(i).setTabList(lists.get(i));
        }

        // mStackRects will get updated in updateLayout()
    }

    @Override
    public boolean forceShowBrowserControlsAndroidView() {
        return true;
    }

    /**
     * Simulates a click on the view at the specified pixel offset
     * from the top left of the view.
     * This is used by UI tests.
     * @param x Coordinate of the click in dp.
     * @param y Coordinate of the click in dp.
     */
    @VisibleForTesting
    public void simulateClick(float x, float y) {
        mGestureHandler.click(x, y, false, -1);
    }

    /**
     * Simulates a drag and issues Up-event to commit the drag.
     * @param x  Coordinate to start the Drag from in dp.
     * @param y  Coordinate to start the Drag from in dp.
     * @param dX Amount of drag in X direction in dp.
     * @param dY Amount of drag in Y direction in dp.
     */
    @VisibleForTesting
    public void simulateDrag(float x, float y, float dX, float dY) {
        mGestureHandler.onDown(x, y, false, -1);
        mGestureHandler.drag(x, y, dX, dY, -1, -1);
        mGestureHandler.onUpOrCancel();
    }

    @Override
    public ViewportMode getViewportMode() {
        return ViewportMode.ALWAYS_FULLSCREEN;
    }

    @Override
    public void setTabModelSelector(TabModelSelector modelSelector, TabContentManager manager) {
        super.setTabModelSelector(modelSelector, manager);
        resetScrollData();
    }

    /**
     * Get the tab stack at the specified index.
     *
     * @param index Which stack should be returned.
     * @return The stack at the specified index.
     * @VisibleForTesting
     */
    public Stack getTabStackAtIndex(int index) {
        return mStacks.get(index);
    }

    /**
     * Get the tab stack state.
     * @return The tab stack index for the given tab id.
     */
    private int getTabStackIndex() {
        return getTabStackIndex(Tab.INVALID_TAB_ID);
    }

    /**
     * Get the tab stack state for the specified tab id.
     *
     * @param tabId The id of the tab to lookup.
     * @return The tab stack index for the given tab id.
     * @VisibleForTesting
     */
    protected abstract int getTabStackIndex(int tabId);

    /**
     * Get the tab stack state for the specified tab id.
     *
     * @param tabId The id of the tab to lookup.
     * @return The tab stack state for the given tab id.
     * @VisibleForTesting
     */
    protected Stack getTabStackForTabId(int tabId) {
        return mStacks.get(getTabStackIndex(tabId));
    }

    /**
     * Commits outstanding model states.
     * @param time  The current time of the app in ms.
     */
    public void commitOutstandingModelState(long time) {
        for (int i = 0; i < mStacks.size(); i++) {
            mStacks.get(i).ensureCleaningUpDyingTabs(time);
        }
    }

    @Override
    public void onTabSelecting(long time, int tabId) {
        commitOutstandingModelState(time);
        if (tabId == Tab.INVALID_TAB_ID) tabId = mTabModelSelector.getCurrentTabId();
        super.onTabSelecting(time, tabId);
        mStacks.get(getTabStackIndex()).tabSelectingEffect(time, tabId);
        startMarginAnimation(false);
        startYOffsetAnimation(false);
        finishScrollStacks();
    }

    @Override
    public void onTabClosing(long time, int id) {
        Stack stack = getTabStackForTabId(id);
        if (stack == null) return;
        stack.tabClosingEffect(time, id);
    }

    @Override
    public void onTabsAllClosing(long time, boolean incognito) {
        super.onTabsAllClosing(time, incognito);
    }

    @Override
    public void onTabClosureCancelled(long time, int id, boolean incognito) {
        super.onTabClosureCancelled(time, id, incognito);
    }

    @Override
    public boolean handlesCloseAll() {
        return true;
    }

    @Override
    public boolean handlesTabCreating() {
        return true;
    }

    @Override
    public boolean handlesTabClosing() {
        return true;
    }

    @Override
    public void attachViews(ViewGroup container) {
        // TODO(dtrainor): This is a hack.  We're attaching to the parent of the view container
        // which is the content container of the Activity.
        ((ViewGroup) container.getParent())
                .addView(mViewContainer,
                        new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
    }

    @Override
    public void detachViews() {
        if (mViewContainer.getParent() != null) {
            ((ViewGroup) mViewContainer.getParent()).removeView(mViewContainer);
        }
        mViewContainer.removeAllViews();
    }

    /**
     * @return A {@link ViewGroup} that {@link Stack}s can use to interact with the Android view
     *         hierarchy.
     */
    public ViewGroup getViewContainer() {
        return mViewContainer;
    }

    @Override
    public boolean onBackPressed() {
        // Force any in progress animations to end. This was introduced because
        // we end up with 0 tabs if the animation for all tabs closing is still
        // running when the back button is pressed. We should finish the animation
        // and close Chrome instead.
        // See http://crbug.com/522447
        onUpdateAnimation(SystemClock.currentThreadTimeMillis(), true);
        return false;
    }

    @Override
    public void onTabCreating(int sourceTabId) {
        // Force any in progress animations to end. This was introduced because
        // we end up with 0 tabs if the animation for all tabs closing is still
        // running when a new tab is created.
        // See http://crbug.com/496557
        onUpdateAnimation(SystemClock.currentThreadTimeMillis(), true);
    }

    @Override
    public void onTabCreated(long time, int id, int tabIndex, int sourceId, boolean newIsIncognito,
            boolean background, float originX, float originY) {
        super.onTabCreated(
                time, id, tabIndex, sourceId, newIsIncognito, background, originX, originY);
        startHiding(id, false);
        mStacks.get(getTabStackIndex(id)).tabCreated(time, id);

        if (FeatureUtilities.isChromeHomeEnabled()) {
            mNewTabLayoutTab = createLayoutTab(id, newIsIncognito, NO_CLOSE_BUTTON, NO_TITLE);
            mNewTabLayoutTab.setScale(1.f);
            mNewTabLayoutTab.setBorderScale(1.f);
            mNewTabLayoutTab.setDecorationAlpha(0.f);
            mNewTabLayoutTab.setY(getHeight() / 2);

            mIsNewTabInitialized = true;

            Interpolator interpolator = BakedBezierInterpolator.TRANSFORM_CURVE;
            addToAnimation(mNewTabLayoutTab, LayoutTab.Property.Y, mNewTabLayoutTab.getY(), 0.f,
                    NEW_TAB_ANIMATION_DURATION_MS, 0, false, interpolator);
        } else {
            startMarginAnimation(false);
        }
    }

    @Override
    public void onTabRestored(long time, int tabId) {
        super.onTabRestored(time, tabId);
        // Call show() so that new stack tabs and potentially new stacks get created.
        // TODO(twellington): add animation for showing the restored tab.
        show(time, false);
    }

    @Override
    public boolean onUpdateAnimation(long time, boolean jumpToEnd) {
        boolean animationsWasDone = super.onUpdateAnimation(time, jumpToEnd);

        boolean finishedAllViews = true;
        for (int i = 0; i < mStacks.size(); i++) {
            finishedAllViews &= mStacks.get(i).onUpdateViewAnimation(time, jumpToEnd);
        }

        boolean finishedAllCompositors = true;
        for (int i = 0; i < mStacks.size(); i++) {
            finishedAllCompositors &= mStacks.get(i).onUpdateCompositorAnimations(time, jumpToEnd);
        }

        if (animationsWasDone && finishedAllViews && finishedAllCompositors) {
            return true;
        } else {
            if (!animationsWasDone || !finishedAllCompositors) {
                requestStackUpdate();
            }

            return false;
        }
    }

    @Override
    protected void onAnimationStarted() {
        if (mStackAnimationCount == 0) super.onAnimationStarted();
    }

    @Override
    protected void onAnimationFinished() {
        if (mStackAnimationCount == 0) super.onAnimationFinished();
        if (mNewTabLayoutTab != null) {
            mIsNewTabInitialized = false;
            mNewTabLayoutTab = null;
        }
    }

    /**
     * Called when a UI element is attempting to select a tab.  This will perform the animation
     * and then actually propagate the action.  This starts hiding this layout which, when complete,
     * will actually select the tab.
     * @param time The current time of the app in ms.
     * @param id   The id of the tab to select.
     */
    public void uiSelectingTab(long time, int id) {
        onTabSelecting(time, id);
    }

    /**
     * Called when a UI element is attempting to close a tab.  This will perform the required close
     * animations.  When the UI is ready to actually close the tab
     * {@link #uiDoneClosingTab(long, int, boolean, boolean)} should be called to actually propagate
     * the event to the model.
     * @param time The current time of the app in ms.
     * @param id   The id of the tab to close.
     */
    public void uiRequestingCloseTab(long time, int id) {
        // Start the tab closing effect if necessary.
        getTabStackForTabId(id).tabClosingEffect(time, id);
    }

    /**
     * Called when a UI element is done animating the close tab effect started by
     * {@link #uiRequestingCloseTab(long, int)}.  This actually pushes the close event to the model.
     * @param time      The current time of the app in ms.
     * @param id        The id of the tab to close.
     * @param canUndo   Whether or not this close can be undone.
     * @param incognito Whether or not this was for the incognito stack or not.
     */
    public void uiDoneClosingTab(long time, int id, boolean canUndo, boolean incognito) {
        // If homepage is enabled and there is a maximum of 1 tab in both models
        // (this is the last tab), the tab closure cannot be undone.
        canUndo &= !(HomepageManager.isHomepageEnabled()
                && (mTabModelSelector.getModel(true).getCount()
                                   + mTabModelSelector.getModel(false).getCount()
                           < 2));

        // Propagate the tab closure to the model.
        TabModelUtils.closeTabById(mTabModelSelector.getModel(incognito), id, canUndo);
    }

    public void uiDoneClosingAllTabs(boolean incognito) {
        // Propagate the tab closure to the model.
        mTabModelSelector.getModel(incognito).closeAllTabs(false, false);
    }

    /**
     * Called when a {@link Stack} instance is done animating the stack enter effect.
     */
    public void uiDoneEnteringStack() {
        mSortingComparator = mVisibilityComparator;
        doneShowing();
    }

    /**
     * Starts the animation for the opposite stack to slide in or out when entering
     * or leaving stack view.  The animation should be super fast to match more or less
     * the fling animation.
     * @param enter True if the stack view is being entered, false if the stack view
     *              is being left.
     */
    protected void startMarginAnimation(boolean enter) {
        startMarginAnimation(enter, mStacks.size() >= 2 && mStacks.get(1).isDisplayable());
    }

    protected void startMarginAnimation(boolean enter, boolean showMargin) {
        // Any outstanding animations must be cancelled to avoid race condition.
        cancelAnimation(this, Property.INNER_MARGIN_PERCENT);

        float start = mInnerMarginPercent;
        float end = enter && showMargin ? 1.0f : 0.0f;
        if (start != end) {
            addToAnimation(this, Property.INNER_MARGIN_PERCENT, start, end, 200, 0);
        }
    }

    private void startYOffsetAnimation(boolean enter) {
        // Any outstanding animations must be cancelled to avoid race condition.
        cancelAnimation(this, Property.STACK_OFFSET_Y_PERCENT);

        float start = mStackOffsetYPercent;
        float end = enter ? 1.f : 0.f;
        if (start != end) {
            addToAnimation(this, Property.STACK_OFFSET_Y_PERCENT, start, end, 300, 0);
        }
    }

    @Override
    public void show(long time, boolean animate) {
        super.show(time, animate);

        Tab tab = mTabModelSelector.getCurrentTab();
        if (tab != null && tab.isNativePage()) mTabContentManager.cacheTabThumbnail(tab);

        // Remove any views in case we're getting another call to show before we hide (quickly
        // toggling the tab switcher button).
        mViewContainer.removeAllViews();
        int currentTabStack = getTabStackIndex();

        for (int i = mStacks.size() - 1; i >= 0; --i) {
            mStacks.get(i).reset();
            if (mStacks.get(i).isDisplayable()) {
                mStacks.get(i).show(i == currentTabStack);
            } else {
                mStacks.get(i).cleanupTabs();
            }
        }
        // Initialize the animation and the positioning of all the elements
        mSortingComparator = mOrderComparator;
        resetScrollData();
        for (int i = mStacks.size() - 1; i >= 0; --i) {
            if (mStacks.get(i).isDisplayable()) {
                boolean offscreen = (i != getTabStackIndex());
                mStacks.get(i).stackEntered(time, !offscreen);
            }
        }
        startMarginAnimation(true);
        startYOffsetAnimation(true);
        flingStacks(getTabStackIndex());

        if (!animate) onUpdateAnimation(time, true);

        // We will render before we get a call to updateLayout.  Need to make sure all of the tabs
        // we need to render are up to date.
        updateLayout(time, 0);
    }

    @Override
    public void swipeStarted(long time, ScrollDirection direction, float x, float y) {
        mStacks.get(getTabStackIndex()).swipeStarted(time, direction, x, y);
    }

    @Override
    public void swipeUpdated(long time, float x, float y, float dx, float dy, float tx, float ty) {
        mStacks.get(getTabStackIndex()).swipeUpdated(time, x, y, dx, dy, tx, ty);
    }

    @Override
    public void swipeFinished(long time) {
        mStacks.get(getTabStackIndex()).swipeFinished(time);
    }

    @Override
    public void swipeCancelled(long time) {
        mStacks.get(getTabStackIndex()).swipeCancelled(time);
    }

    @Override
    public void swipeFlingOccurred(
            long time, float x, float y, float tx, float ty, float vx, float vy) {
        mStacks.get(getTabStackIndex()).swipeFlingOccurred(time, x, y, tx, ty, vx, vy);
    }

    private void requestStackUpdate() {
        // TODO(jgreenwald): It isn't always necessary to invalidate all stacks.
        for (int i = 0; i < mStacks.size(); i++) {
            mStacks.get(i).requestUpdate();
        }
    }

    @Override
    public void notifySizeChanged(float width, float height, int orientation) {
        mWidth = width;
        mHeight = height;
        mOrientation = orientation;
        mCachedLandscapeViewport = null;
        mCachedPortraitViewport = null;
        for (Stack stack : mStacks) {
            stack.notifySizeChanged(width, height, orientation);
        }
        resetScrollData();
        requestStackUpdate();
    }

    @Override
    public void contextChanged(Context context) {
        super.contextChanged(context);
        StackTab.resetDimensionConstants(context);
        for (Stack stack : mStacks) {
            stack.contextChanged(context);
        }
        requestStackUpdate();
    }

    protected int getMinRenderedScrollOffset() {
        return -(mStacks.size() - 1);
    }

    /**
     * Computes the input mode for drag and fling based on the first event position.
     * @param time The current time of the app in ms.
     * @param x    The x layout position of the mouse (without the displacement).
     * @param y    The y layout position of the mouse (without the displacement).
     * @param dx   The x displacement happening this frame.
     * @param dy   The y displacement happening this frame.
     * @return     The input mode to select.
     */
    protected @SwipeMode int computeInputMode(long time, float x, float y, float dx, float dy) {
        if (mStacks.size() == 0) return SWIPE_MODE_NONE;
        if (mStacks.size() == 1) return SWIPE_MODE_SEND_TO_STACK;

        int currentIndex = getTabStackIndex();

        // When a drag starts, lock the drag into being either horizontal or vertical until the
        // next touch down. The deltas here are already verified by StackLayoutGestureHandler as
        // being above some threshold so that we know we're handling a drag or fling and not a long
        // press.
        if (mDragDirection == DRAG_DIRECTION_NONE) {
            if (Math.abs(dx) > Math.abs(dy)) {
                mDragDirection = DRAG_DIRECTION_HORIZONTAL;
            } else {
                mDragDirection = DRAG_DIRECTION_VERTICAL;
            }
        }

        if ((mDragDirection == DRAG_DIRECTION_VERTICAL) ^ isUsingHorizontalLayout()) {
            return SWIPE_MODE_SEND_TO_STACK;
        }

        float relativeX = mLastOnDownX - (x + dx);
        float relativeY = mLastOnDownY - (y + dy);
        float switchDelta = isUsingHorizontalLayout() ? relativeY : relativeX;

        // In LTR portrait mode, the first stack can be swiped to the left to switch to the second
        // stack, and the last stack can be swiped to the right to switch to the first stack. We
        // reverse the check for RTL portrait mode because increasing the stack index corresponds
        // to a negative switchDelta. If there are more than two stacks, we do not currently support
        // swiping to close on any of the stacks in the middle
        //
        // Landscape mode is like LTR portrait mode (increasing the stack index corresponds to a
        // positive switchDelta).
        final boolean isRtlPortraitMode =
                (!isUsingHorizontalLayout() && LocalizationUtils.isLayoutRtl());
        final boolean onLeftmostStack = (currentIndex == 0 && !isRtlPortraitMode)
                || (currentIndex == mStacks.size() - 1 && isRtlPortraitMode);
        final boolean onRightmostStack = (currentIndex == 0 && isRtlPortraitMode)
                || (currentIndex == mStacks.size() - 1 && !isRtlPortraitMode);
        if ((onLeftmostStack && switchDelta < 0) || (onRightmostStack && switchDelta > 0)) {
            // Dragging in a direction the stack cannot switch. Pass the drag to the Stack, which
            // will treat it as intending to discard a tab.
            return SWIPE_MODE_SEND_TO_STACK;
        } else {
            // Interpret the drag as intending to switch between tab stacks.
            return SWIPE_MODE_SWITCH_STACK;
        }
    }

    class PortraitViewport {
        protected float mWidth, mHeight;
        PortraitViewport() {
            mWidth = StackLayoutBase.this.getWidth();
            mHeight = StackLayoutBase.this.getHeightMinusBrowserControls();
        }

        float getClampedRenderedScrollOffset() {
            return MathUtils.clamp(mRenderedScrollOffset, 0, getMinRenderedScrollOffset());
        }

        float getInnerMargin() {
            float margin = mInnerMarginPercent
                    * Math.max(mMinMaxInnerMargin, mWidth * INNER_MARGIN_PERCENT_PERCENT);
            return margin;
        }

        /**
         * Returns an offset that can be added to the index of the current stack to get the index of
         * the stack at the specified on-screen location.
         * @param x The x coordinate of the specified on-screen location.
         * @param y The x coordinate of the specified on-screen location.
         * @return  The offset to be added to the index of the current stack.
         */
        int getStackIndexDeltaAt(float x, float y) {
            int delta = 0;
            if (x < getCurrentStackLeft()) {
                delta = -1;
            } else if (x > getCurrentStackLeft() + getWidth()) {
                delta = 1;
            }

            // Tabs are counted from left to right in LTR mode, but from right to left in RTL mode.
            if (LocalizationUtils.isLayoutRtl()) delta *= -1;

            return delta;
        }

        /**
         * @return The current x coordinate for the left edge of the first stack (right edge if in
         * RTL mode).
         */
        float getStack0Left() {
            float stack0LeftLtr = getClampedRenderedScrollOffset() * getFullScrollDistance();
            if (mStacks.size() > 2) {
                // If we have one or two stacks, we only show a margin on the right side of the left
                // stack and on the left side of the right stack. But if we have three or more
                // stacks, we put a margin on both sides
                stack0LeftLtr += getInnerMargin() / 2;
            }

            if (LocalizationUtils.isLayoutRtl()) return getInnerMargin() - stack0LeftLtr;

            return stack0LeftLtr;
        }

        /**
         * @return The current x coordinate for the left edge of the current stack (actually the
         * right edge if in RTL mode).
         */
        float getCurrentStackLeft() {
            float offset = getClampedRenderedScrollOffset() + getTabStackIndex();
            if (mStacks.size() > 2) {
                return offset * getFullScrollDistance() + getInnerMargin() / 2;
            }

            // Note: getInnerMargin() is zero if there's only one stack.
            boolean isRightStack = (getTabStackIndex() == 1) ^ LocalizationUtils.isLayoutRtl();
            return offset * getFullScrollDistance() + (isRightStack ? getInnerMargin() : 0);
        }

        float getWidth() {
            return mWidth - getInnerMargin();
        }

        float getHeight() {
            return mHeight;
        }

        float getStack0Top() {
            return getTopHeightOffset();
        }

        float getStack0ToStack1TranslationX() {
            return Math.round(LocalizationUtils.isLayoutRtl() ? -mWidth + getInnerMargin()
                                                              : mWidth - getInnerMargin());
        }

        float getStack0ToStack1TranslationY() {
            return 0.0f;
        }

        float getTopHeightOffset() {
            if (FeatureUtilities.isChromeHomeEnabled()) return MODERN_TOP_MARGIN_DP;
            return (StackLayoutBase.this.getHeight() - getHeightMinusBrowserControls())
                    * mStackOffsetYPercent;
        }
    }

    class LandscapeViewport extends PortraitViewport {
        LandscapeViewport() {
            // This is purposefully inverted.
            mWidth = StackLayoutBase.this.getHeightMinusBrowserControls();
            mHeight = StackLayoutBase.this.getWidth();
        }

        @Override
        float getInnerMargin() {
            float margin = mInnerMarginPercent
                    * Math.max(mMinMaxInnerMargin, mWidth * INNER_MARGIN_PERCENT_PERCENT);
            return margin;
        }

        @Override
        int getStackIndexDeltaAt(float x, float y) {
            if (y < getCurrentStackTop()) return -1;
            if (y > getCurrentStackTop() + getHeight()) return 1;
            return 0;
        }

        @Override
        float getStack0Left() {
            return 0.f;
        }

        @Override
        float getStack0Top() {
            return getClampedRenderedScrollOffset() * getFullScrollDistance()
                    + getTopHeightOffset();
        }

        /**
         * @return The current y coordinate for the top edge of the current stack.
         */
        float getCurrentStackTop() {
            float offset = getClampedRenderedScrollOffset() + getTabStackIndex();
            if (mStacks.size() > 2) {
                return offset * getFullScrollDistance() + getInnerMargin() / 2
                        + getTopHeightOffset();
            }

            return offset * getFullScrollDistance()
                    + ((getTabStackIndex() == 1) ? getInnerMargin() : 0) + getTopHeightOffset();
        }

        @Override
        float getWidth() {
            return super.getHeight();
        }

        @Override
        float getHeight() {
            return super.getWidth();
        }

        @Override
        float getStack0ToStack1TranslationX() {
            return super.getStack0ToStack1TranslationY();
        }

        @Override
        float getStack0ToStack1TranslationY() {
            return Math.round(mWidth - getInnerMargin());
        }
    }

    private PortraitViewport getViewportParameters() {
        if (isUsingHorizontalLayout()) {
            if (mCachedLandscapeViewport == null) {
                mCachedLandscapeViewport = new LandscapeViewport();
            }
            return mCachedLandscapeViewport;
        } else {
            if (mCachedPortraitViewport == null) {
                mCachedPortraitViewport = new PortraitViewport();
            }
            return mCachedPortraitViewport;
        }
    }

    /**
     * Scrolls the tab stacks by amount delta (clamped so that it's not possible to scroll past the
     * last stack in either direciton). Positive delta corresponds to increasing the x coordinate
     * in portrait mode (in both LTR and RTL modes), or increasing the y coordinate in landscape
     * mode.
     * @param delta The amount to scroll by.
     */
    private void scrollStacks(float delta) {
        cancelAnimation(this, Property.STACK_SNAP);
        float fullDistance = getFullScrollDistance();
        mScrollIndexOffset += MathUtils.flipSignIf(delta / fullDistance,
                !isUsingHorizontalLayout() && LocalizationUtils.isLayoutRtl());
        mRenderedScrollOffset =
                MathUtils.clamp(mScrollIndexOffset, 0, getMinRenderedScrollOffset());
        requestStackUpdate();
    }

    /**
     * Scrolls over to the tab stack at the specified index, and records that it's now the current
     * tab stack.
     * @param index The index of the newly-selected tab stack.
     */
    protected void flingStacks(int index) {
        setActiveStackState(index);
        finishScrollStacks();
        requestStackUpdate();
    }

    /**
     * Animate to the final position of the stack.  Unfortunately, both touch-up
     * and fling can be called and this depends on fling always being called last.
     * If fling is called first, onUpOrCancel can override the fling position
     * with the opposite.  For example, if the user does a very small fling from
     * incognito to non-incognito, which leaves the up event in the incognito side.
     */
    private void finishScrollStacks() {
        cancelAnimation(this, Property.STACK_SNAP);
        final int currentModelIndex = getTabStackIndex();
        float delta = Math.abs(currentModelIndex + mRenderedScrollOffset);
        float target = -currentModelIndex;
        if (delta != 0) {
            long duration = FLING_MIN_DURATION
                    + (long) Math.abs(delta * getFullScrollDistance() / mFlingSpeed);
            addToAnimation(this, Property.STACK_SNAP, mRenderedScrollOffset, target, duration, 0);
        } else {
            setProperty(Property.STACK_SNAP, target);
            onAnimationFinished();
        }
    }

    /**
     * Pushes a rectangle to be drawn on the screen on top of everything.
     *
     * @param rect  The rectangle to be drawn on screen
     * @param color The color of the rectangle
     */
    public void pushDebugRect(Rect rect, int color) {
        if (rect.left > rect.right) {
            int tmp = rect.right;
            rect.right = rect.left;
            rect.left = tmp;
        }
        if (rect.top > rect.bottom) {
            int tmp = rect.bottom;
            rect.bottom = rect.top;
            rect.top = tmp;
        }
        mRenderHost.pushDebugRect(rect, color);
    }

    @Override
    protected void updateLayout(long time, long dt) {
        super.updateLayout(time, dt);
        boolean needUpdate = false;

        if (mStackRects.size() > mStacks.size()) {
            mStackRects.subList(mStacks.size(), mStackRects.size()).clear();
        }
        while (mStackRects.size() < mStacks.size()) mStackRects.add(new RectF());

        final PortraitViewport viewport = getViewportParameters();

        if (!mStackRects.isEmpty()) {
            mStackRects.get(0).left = viewport.getStack0Left();
            mStackRects.get(0).right = mStackRects.get(0).left + viewport.getWidth();
            mStackRects.get(0).top = viewport.getStack0Top();
            mStackRects.get(0).bottom = mStackRects.get(0).top + viewport.getHeight();
        }

        for (int i = 1; i < mStackRects.size(); i++) {
            mStackRects.get(i).left =
                    mStackRects.get(i - 1).left + viewport.getStack0ToStack1TranslationX();
            mStackRects.get(i).right = mStackRects.get(i).left + viewport.getWidth();
            mStackRects.get(i).top =
                    mStackRects.get(i - 1).top + viewport.getStack0ToStack1TranslationY();
            mStackRects.get(i).bottom = mStackRects.get(i).top + viewport.getHeight();
        }

        for (int i = 0; i < mStacks.size(); i++) {
            final float scrollDistance = Math.abs(i + mRenderedScrollOffset);
            final float stackFocus = MathUtils.clamp(1 - scrollDistance, 0, 1);

            mStacks.get(i).setStackFocusInfo(stackFocus,
                    mSortingComparator == mOrderComparator ? mStacks.get(i).getTabList().index()
                                                           : -1);
        }

        // Compute position and visibility
        for (int i = 0; i < mStacks.size(); i++) {
            mStacks.get(i).computeTabPosition(time, mStackRects.get(i));
        }

        // Pre-allocate/resize {@link #mLayoutTabs} before it get populated by
        // computeTabPositionAndAppendLayoutTabs.
        int tabVisibleCount = 0;
        for (int i = 0; i < mStacks.size(); i++) {
            tabVisibleCount += mStacks.get(i).getVisibleCount();
        }

        int layoutTabCount = tabVisibleCount + (mNewTabLayoutTab == null ? 0 : 1);

        if (layoutTabCount == 0) {
            mLayoutTabs = null;
        } else if (mLayoutTabs == null || mLayoutTabs.length != layoutTabCount) {
            mLayoutTabs = new LayoutTab[layoutTabCount];
        }

        int index = 0;
        for (int i = 0; i < mStacks.size(); i++) {
            // Append tabs for the current stack last so they get priority in rendering.
            if (getTabStackIndex() == i) continue;
            index = appendVisibleLayoutTabs(time, i, mLayoutTabs, index);
        }
        index = appendVisibleLayoutTabs(time, getTabStackIndex(), mLayoutTabs, index);
        assert index == tabVisibleCount : "index should be incremented up to tabVisibleCount";

        // Update tab snapping
        for (int i = 0; i < tabVisibleCount; i++) {
            if (mLayoutTabs[i].updateSnap(dt)) needUpdate = true;
        }

        if (mNewTabLayoutTab != null && mIsNewTabInitialized) {
            mLayoutTabs[mLayoutTabs.length - 1] = mNewTabLayoutTab;
            if (mNewTabLayoutTab.updateSnap(dt)) needUpdate = true;
        }

        if (needUpdate) requestUpdate();

        // Since we've updated the positions of the stacks and tabs, let's go ahead and update
        // the visible tabs.
        updateTabPriority();
    }

    private int appendVisibleLayoutTabs(long time, int stackIndex, LayoutTab[] tabs, int tabIndex) {
        final StackTab[] stackTabs = mStacks.get(stackIndex).getTabs();
        if (stackTabs != null) {
            for (int i = 0; i < stackTabs.length; i++) {
                LayoutTab t = stackTabs[i].getLayoutTab();
                if (t.isVisible()) tabs[tabIndex++] = t;
            }
        }
        return tabIndex;
    }

    /**
     * Sets the active tab stack.
     *
     * @param stackIndex Index of the tab stack to be made active.
     */
    public void setActiveStackState(int stackIndex) {
        mTemporarySelectedStack = stackIndex;
    }

    private void resetScrollData() {
        mScrollIndexOffset = -getTabStackIndex();
        mRenderedScrollOffset = mScrollIndexOffset;
    }

    /**
     * @return The distance between two neighboring tab stacks.
     */
    private float getFullScrollDistance() {
        float distance = isUsingHorizontalLayout() ? getHeightMinusBrowserControls() : getWidth();
        if (mStacks.size() > 2) {
            return distance - getViewportParameters().getInnerMargin();
        }

        return distance - 2 * getViewportParameters().getInnerMargin();
    }

    @Override
    public void doneHiding() {
        super.doneHiding();

        mInnerMarginPercent = 0.0f;
        mStackOffsetYPercent = 0.0f;
        mTabModelSelector.commitAllTabClosures();
    }

    /**
     * Extracts the tabs from a stack and append them into a list.
     * @param stack     The stack that contains the tabs.
     * @param outList   The output list where will be the tabs from the stack.
     * @param index     The current number of item in the outList.
     * @return The updated index incremented by the number of tabs in the stack.
     */
    private static int addAllTabs(Stack stack, StackTab[] outList, int index) {
        StackTab[] stackTabs = stack.getTabs();
        if (stackTabs != null) {
            for (int i = 0; i < stackTabs.length; ++i) {
                outList[index++] = stackTabs[i];
            }
        }
        return index;
    }

    /**
     * Comparator that helps ordering StackTab's visibility sorting value in a decreasing order.
     */
    private static class VisibilityComparator implements Comparator<StackTab>, Serializable {
        @Override
        public int compare(StackTab tab1, StackTab tab2) {
            return (int) (tab2.getVisiblitySortingValue() - tab1.getVisiblitySortingValue());
        }
    }

    /**
     * Comparator that helps ordering StackTab's visibility sorting value in a decreasing order.
     */
    private static class OrderComparator implements Comparator<StackTab>, Serializable {
        @Override
        public int compare(StackTab tab1, StackTab tab2) {
            return tab1.getOrderSortingValue() - tab2.getOrderSortingValue();
        }
    }

    /**
     * Updates mSortedPriorityArray, which stores the list of StackTabs to render, sorted by
     * rendering priority.
     *
     * @param comparator The comparator used to sort the StackTabs.
     * @return True if at least one Stack has a tab, false if there are no tabs.
     */
    private boolean updateSortedPriorityArray(Comparator<StackTab> comparator) {
        int allTabsCount = 0;
        for (int i = 0; i < mStacks.size(); i++) {
            allTabsCount += mStacks.get(i).getCount();
        }
        if (allTabsCount == 0) return false;
        if (mSortedPriorityArray == null || mSortedPriorityArray.length != allTabsCount) {
            mSortedPriorityArray = new StackTab[allTabsCount];
        }
        int sortedOffset = 0;
        for (int i = 0; i < mStacks.size(); i++) {
            sortedOffset = addAllTabs(mStacks.get(i), mSortedPriorityArray, sortedOffset);
        }
        assert sortedOffset == mSortedPriorityArray.length;
        Arrays.sort(mSortedPriorityArray, comparator);
        return true;
    }

    /**
     * Updates the priority list of the {@link LayoutTab} and sends it the systems having processing
     * to do on a per {@link LayoutTab} basis. Priority meaning may change based on the current
     * comparator stored in {@link #mSortingComparator}.
     *
     * Do not use {@link #mSortedPriorityArray} out side this context. It is only a member to avoid
     * doing an allocation every frames.
     */
    private void updateTabPriority() {
        if (!updateSortedPriorityArray(mSortingComparator)) return;
        updateTabsVisibility(mSortedPriorityArray);
        updateDelayedLayoutTabInit(mSortedPriorityArray);
    }

    /**
     * Updates the list of visible tab Id that the tab content manager is suppose to serve. The list
     * is ordered by priority. The first ones must be in the manager, then the remaining ones should
     * have at least approximations if possible.
     *
     * @param sortedPriorityArray The array of all the {@link StackTab} sorted by priority.
     */
    private void updateTabsVisibility(StackTab[] sortedPriorityArray) {
        mVisibilityArray.clear();
        for (int i = 0; i < sortedPriorityArray.length; i++) {
            mVisibilityArray.add(sortedPriorityArray[i].getId());
        }
        updateCacheVisibleIds(mVisibilityArray);
    }

    /**
     * Initializes the {@link LayoutTab} a few at a time. This function is to be called once a
     * frame.
     * The logic of that function is not as trivial as it should be because the input array we want
     * to initialize the tab from keeps getting reordered from calls to call. This is needed to
     * get the highest priority tab initialized first.
     *
     * @param sortedPriorityArray The array of all the {@link StackTab} sorted by priority.
     */
    private void updateDelayedLayoutTabInit(StackTab[] sortedPriorityArray) {
        if (!mDelayedLayoutTabInitRequired) return;

        int initialized = 0;
        final int count = sortedPriorityArray.length;
        for (int i = 0; i < count; i++) {
            if (initialized >= LAYOUTTAB_ASYNCHRONOUS_INITIALIZATION_BATCH_SIZE) return;

            LayoutTab layoutTab = sortedPriorityArray[i].getLayoutTab();
            // The actual initialization is done by the parent class.
            if (super.initLayoutTabFromHost(layoutTab)) {
                initialized++;
            }
        }
        if (initialized == 0) mDelayedLayoutTabInitRequired = false;
    }

    @Override
    protected boolean initLayoutTabFromHost(LayoutTab layoutTab) {
        if (layoutTab.isInitFromHostNeeded()) mDelayedLayoutTabInitRequired = true;
        return false;
    }

    /**
     * Sets properties for animations.
     * @param prop The property to update
     * @param p New value of the property
     */
    @Override
    public void setProperty(Property prop, float p) {
        switch (prop) {
            case STACK_SNAP:
                mRenderedScrollOffset = p;
                mScrollIndexOffset = p;
                break;
            case INNER_MARGIN_PERCENT:
                mInnerMarginPercent = p;
                break;
            case STACK_OFFSET_Y_PERCENT:
                mStackOffsetYPercent = p;
                break;
        }
    }

    @Override
    public void onPropertyAnimationFinished(Property prop) {}

    /**
     * Called by the stacks whenever they start an animation.
     */
    public void onStackAnimationStarted() {
        if (mStackAnimationCount == 0) super.onAnimationStarted();
        mStackAnimationCount++;
    }

    /**
     * Called by the stacks whenever they finish their animations.
     */
    public void onStackAnimationFinished() {
        mStackAnimationCount--;
        if (mStackAnimationCount == 0) super.onAnimationFinished();
    }

    @Override
    protected EventFilter getEventFilter() {
        return mGestureEventFilter;
    }

    @Override
    protected SceneLayer getSceneLayer() {
        return mSceneLayer;
    }

    @Override
    protected void updateSceneLayer(RectF viewport, RectF contentViewport,
            LayerTitleCache layerTitleCache, TabContentManager tabContentManager,
            ResourceManager resourceManager, ChromeFullscreenManager fullscreenManager) {
        super.updateSceneLayer(viewport, contentViewport, layerTitleCache, tabContentManager,
                resourceManager, fullscreenManager);
        assert mSceneLayer != null;

        mSceneLayer.pushLayers(getContext(), viewport, contentViewport, this, layerTitleCache,
                tabContentManager, resourceManager, fullscreenManager);
    }
}

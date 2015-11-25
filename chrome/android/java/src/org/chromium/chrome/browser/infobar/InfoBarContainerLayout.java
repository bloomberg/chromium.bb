// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.InfoBarContainer.InfoBarAnimationListener;

import java.util.ArrayList;
import java.util.Collection;

/**
 * Layout that displays infobars in a stack. Handles all the animations when adding or removing
 * infobars and when swapping infobar contents.
 *
 * The first infobar to be added is visible at the front of the stack. Later infobars peek up just
 * enough behind the front infobar to signal their existence; their contents aren't visible at all.
 * The stack has a max depth of three infobars. If additional infobars are added beyond this, they
 * won't be visible at all until infobars in front of them are dismissed.
 *
 * Animation details:
 *  - Newly added infobars slide up from the bottom and then their contents fade in.
 *  - Disappearing infobars slide down and away. The remaining infobars, if any, resize to the
 *    new front infobar's size, then the content of the new front infobar fades in.
 *  - When swapping the front infobar's content, the old content fades out, the infobar resizes to
 *    the new content's size, then the new content fades in.
 *  - Only a single animation happens at a time. If several infobars are added and/or removed in
 *    quick succession, the animations will be queued and run sequentially.
 *
 * Note: this class depends only on Android view code; it intentionally does not depend on any other
 * infobar code. This is an explicit design decision and should remain this way.
 *
 * TODO(newt): what happens when detached from window? Do animations run? Do animations jump to end
 *     values? Should they jump to end values? Does requestLayout() get called when detached
 *     from window? Probably not; it probably just gets called later when reattached.
 *
 * TODO(newt): use hardware acceleration? See
 *     http://blog.danlew.net/2015/10/20/using-hardware-layers-to-improve-animation-performance/
 *     and http://developer.android.com/guide/topics/graphics/hardware-accel.html#layers
 *
 * TODO(newt): handle tall infobars on small devices. Use a ScrollView inside the InfoBarWrapper?
 *     Make sure InfoBarContainerLayout doesn't extend into tabstrip on tablet.
 *
 * TODO(newt): Disable key events during animations, perhaps by overriding dispatchKeyEvent().
 *     Or can we just call setEnabled() false on the infobar wrapper? Will this cause the buttons
 *     visual state to change (i.e. to turn gray)?
 *
 * TODO(newt): finalize animation timings.
 *
 * TODO: investigate major lag on Nexus 7 runing KK when tapping e.g. "French" on the translation
 *     infobar to trigger the Change Languages panel.
 */
class InfoBarContainerLayout extends FrameLayout {

    /**
     * An interface for items that can be added to an InfoBarContainerLayout.
     */
    interface Item {
        /**
         * Returns the View that represents this infobar. This should have no background or borders;
         * a background and shadow will be added by a wrapper view.
         */
        View getView();

        /**
         * Returns whether controls for this View should be clickable. If false, all input events on
         * this item will be ignored.
         */
        boolean areControlsEnabled();

        /**
         * Sets whether or not controls for this View should be clickable. This does not affect the
         * visual state of the infobar.
         * @param state If false, all input events on this Item will be ignored.
         */
        void setControlsEnabled(boolean state);

        /**
         * Returns the accessibility text to announce when this infobar is first shown.
         */
        CharSequence getAccessibilityText();
    }

    /**
     * Creates an empty InfoBarContainerLayout.
     */
    InfoBarContainerLayout(Context context) {
        super(context);
        setChildrenDrawingOrderEnabled(true);
        mBackInfobarHeight = context.getResources().getDimensionPixelSize(
                R.dimen.infobar_peeking_height);
    }

    /**
     * Adds an infobar to the container. The infobar appearing animation will happen after the
     * current animation, if any, finishes.
     */
    void addInfoBar(Item item) {
        mItems.add(item);
        processPendingAnimations();
    }

    /**
     * Removes an infobar from the container. The infobar will be animated off the screen if it's
     * currently visible.
     */
    void removeInfoBar(Item item) {
        mItems.remove(item);
        processPendingAnimations();
    }

    /**
     * Removes multiple infobars simultaneously. This method (as opposed to calling removeInfoBar()
     * multiple times) ensures that the animations will happen in the optimal order, i.e. back
     * infobars disappear first.
     */
    void removeInfoBars(Collection<Item> items) {
        mItems.removeAll(items);
        processPendingAnimations();
    }

    /**
     * Notifies that an infobar's View ({@link Item#getView}) has changed. If the
     * infobar is visible in the front of the stack, the infobar will fade out the old contents,
     * resize, then fade in the new contents.
     */
    void notifyInfoBarViewChanged() {
        processPendingAnimations();
    }

    /**
     * Returns true if any animations are pending or in progress.
     */
    boolean isAnimating() {
        return mAnimation != null;
    }

    /**
     * Sets a listener to receive updates when each animation is complete.
     */
    void setAnimationListener(InfoBarAnimationListener listener) {
        mAnimationListener = listener;
    }

    /////////////////////////////////////////
    // Implementation details
    /////////////////////////////////////////

    /** The maximum number of infobars visible at any time. */
    private static final int MAX_STACK_DEPTH = 3;

    // Animation durations.
    private static final int DURATION_SLIDE_UP_MS = 250;
    private static final int DURATION_SLIDE_DOWN_MS = 250;
    private static final int DURATION_FADE_MS = 100;
    private static final int DURATION_FADE_OUT_MS = 200;

    /**
     * Base class for animations inside the InfoBarContainerLayout.
     *
     * Provides a standardized way to prepare for, run, and clean up after animations. Each subclass
     * should implement prepareAnimation(), createAnimator(), and onAnimationEnd() as needed.
     */
    private abstract class InfoBarAnimation {
        private Animator mAnimator;

        final boolean isStarted() {
            return mAnimator != null;
        }

        final void start() {
            Animator.AnimatorListener listener = new AnimatorListenerAdapter() {
                @Override
                public void onAnimationEnd(Animator animation) {
                    mAnimation = null;
                    InfoBarAnimation.this.onAnimationEnd();
                    if (mAnimationListener != null) {
                        mAnimationListener.notifyAnimationFinished(getAnimationType());
                    }
                    processPendingAnimations();
                }
            };

            mAnimator = createAnimator();
            mAnimator.addListener(listener);
            mAnimator.start();
        }

        /**
         * Called before the animation begins. This is the time to add views to the hierarchy and
         * adjust layout parameters.
         */
        void prepareAnimation() {}

        /**
         * Called to create an Animator which will control the animation. Called after
         * prepareAnimation() and after a subsequent layout has happened.
         */
        abstract Animator createAnimator();

        /**
         * Called after the animation completes. This is the time to do post-animation cleanup, such
         * as removing views from the hierarchy.
         */
        void onAnimationEnd() {}

        /**
         * Returns the InfoBarAnimationListener.ANIMATION_TYPE_* constant that corresponds to this
         * type of animation (showing, swapping, etc).
         */
        abstract int getAnimationType();
    }

    /**
     * The animation to show the first infobar. The infobar slides up from the bottom; then its
     * content fades in.
     */
    private class FrontInfoBarAppearingAnimation extends InfoBarAnimation {
        private ViewGroup mFrontView;
        private View mFrontInnerView;

        FrontInfoBarAppearingAnimation(View frontInnerView) {
            mFrontInnerView = frontInnerView;
        }

        @Override
        void prepareAnimation() {
            mFrontView = (ViewGroup) LayoutInflater.from(getContext()).inflate(
                    R.layout.infobar_wrapper, InfoBarContainerLayout.this, false);
            mFrontView.addView(mFrontInnerView);
            addView(mFrontView);
            updateLayoutParams();
        }

        @Override
        Animator createAnimator() {
            mFrontView.setTranslationY(mFrontView.getHeight());
            mFrontInnerView.setAlpha(0f);

            AnimatorSet animator = new AnimatorSet();
            animator.playSequentially(
                    ObjectAnimator.ofFloat(mFrontView, View.TRANSLATION_Y, 0f)
                            .setDuration(DURATION_SLIDE_UP_MS),
                    ObjectAnimator.ofFloat(mFrontInnerView, View.ALPHA, 1f)
                            .setDuration(DURATION_FADE_MS));
            return animator;
        }

        @Override
        void onAnimationEnd() {
            announceForAccessibility(mFrontItem.getAccessibilityText());
        }

        @Override
        int getAnimationType() {
            return InfoBarAnimationListener.ANIMATION_TYPE_SHOW;
        }
    }

    /**
     * The animation to show a back infobar. The infobar slides up behind the existing infobars, so
     * its top edge peeks out just a bit.
     */
    private class BackInfoBarAppearingAnimation extends InfoBarAnimation {
        private View mAppearingView;

        @Override
        void prepareAnimation() {
            mAppearingView = LayoutInflater.from(getContext()).inflate(R.layout.infobar_wrapper,
                    InfoBarContainerLayout.this, false);
            addView(mAppearingView);
            updateLayoutParams();
        }

        @Override
        Animator createAnimator() {
            mAppearingView.setTranslationY(mAppearingView.getHeight());
            return ObjectAnimator.ofFloat(mAppearingView, View.TRANSLATION_Y, 0f)
                    .setDuration(DURATION_SLIDE_UP_MS);
        }

        @Override
        int getAnimationType() {
            return InfoBarAnimationListener.ANIMATION_TYPE_SHOW;
        }
    }

    /**
     * The animation to hide the front infobar and reveal the second-to-front infobar. The front
     * infobar slides down and off the screen. The back infobar(s) will adjust to the size of the
     * new front infobar, and then the new front infobar's contents will fade in.
     */
    private class FrontInfoBarDisappearingAndRevealingAnimation extends InfoBarAnimation {
        private ViewGroup mOldFrontView;
        private ViewGroup mNewFrontView;
        private View mNewFrontInnerView;

        FrontInfoBarDisappearingAndRevealingAnimation(View newFrontInnerView) {
            mNewFrontInnerView = newFrontInnerView;
        }

        @Override
        void prepareAnimation() {
            mOldFrontView = (ViewGroup) getChildAt(0);
            mNewFrontView = (ViewGroup) getChildAt(1);
            mNewFrontView.addView(mNewFrontInnerView);
        }

        @Override
        Animator createAnimator() {
            // The amount by which mNewFrontView will grow (a negative value indicates shrinking).
            int deltaHeight = (mNewFrontView.getHeight() - mBackInfobarHeight)
                    - mOldFrontView.getHeight();
            int startTranslationY = Math.max(deltaHeight, 0);
            int endTranslationY = Math.max(-deltaHeight, 0);

            // Slide the front infobar down and away.
            AnimatorSet animator = new AnimatorSet();
            mOldFrontView.setTranslationY(startTranslationY);
            animator.play(ObjectAnimator.ofFloat(mOldFrontView, View.TRANSLATION_Y,
                    startTranslationY + mOldFrontView.getHeight())
                    .setDuration(DURATION_SLIDE_UP_MS));

            // Slide the other infobars to their new positions.
            // Note: animator.play() causes these animations to run simultaneously.
            for (int i = 1; i < getChildCount(); i++) {
                getChildAt(i).setTranslationY(startTranslationY);
                animator.play(ObjectAnimator.ofFloat(getChildAt(i), View.TRANSLATION_Y,
                        endTranslationY).setDuration(DURATION_SLIDE_UP_MS));
            }

            mNewFrontInnerView.setAlpha(0f);
            animator.play(ObjectAnimator.ofFloat(mNewFrontInnerView, View.ALPHA, 1f)
                    .setDuration(DURATION_FADE_MS)).after(DURATION_SLIDE_UP_MS);

            return animator;
        }

        @Override
        void onAnimationEnd() {
            mOldFrontView.removeAllViews();
            removeView(mOldFrontView);
            for (int i = 0; i < getChildCount(); i++) {
                getChildAt(i).setTranslationY(0);
            }
            updateLayoutParams();
            announceForAccessibility(mFrontItem.getAccessibilityText());
        }

        @Override
        int getAnimationType() {
            return InfoBarAnimationListener.ANIMATION_TYPE_HIDE;
        }
    }

    /**
     * The animation to hide the backmost infobar, or the front infobar if there's only one infobar.
     * The infobar simply slides down out of the container.
     */
    private class InfoBarDisappearingAnimation extends InfoBarAnimation {
        private ViewGroup mDisappearingView;

        @Override
        void prepareAnimation() {
            mDisappearingView = (ViewGroup) getChildAt(getChildCount() - 1);
        }

        @Override
        Animator createAnimator() {
            return ObjectAnimator.ofFloat(mDisappearingView, View.TRANSLATION_Y,
                    mDisappearingView.getHeight())
                    .setDuration(DURATION_SLIDE_DOWN_MS);
        }

        @Override
        void onAnimationEnd() {
            mDisappearingView.removeAllViews();
            removeView(mDisappearingView);
            updateLayoutParams();
        }

        @Override
        int getAnimationType() {
            return InfoBarAnimationListener.ANIMATION_TYPE_HIDE;
        }
    }

    /**
     * The animation to swap the contents of the front infobar. The current contents fade out,
     * then the infobar resizes to fit the new contents, then the new contents fade in.
     */
    private class FrontInfoBarSwapContentsAnimation extends InfoBarAnimation {
        private ViewGroup mFrontView;
        private View mOldInnerView;
        private View mNewInnerView;

        FrontInfoBarSwapContentsAnimation(View newInnerView) {
            mNewInnerView = newInnerView;
        }

        @Override
        void prepareAnimation() {
            mFrontView = (ViewGroup) getChildAt(0);
            mOldInnerView = mFrontView.getChildAt(0);
            mFrontView.addView(mNewInnerView);
        }

        @Override
        Animator createAnimator() {
            int deltaHeight = mNewInnerView.getHeight() - mOldInnerView.getHeight();
            InfoBarContainerLayout.this.setTranslationY(Math.max(0, deltaHeight));
            mNewInnerView.setAlpha(0f);

            AnimatorSet animator = new AnimatorSet();
            animator.playSequentially(
                    ObjectAnimator.ofFloat(mOldInnerView, View.ALPHA, 0f)
                            .setDuration(DURATION_FADE_OUT_MS),
                    ObjectAnimator.ofFloat(InfoBarContainerLayout.this, View.TRANSLATION_Y,
                            Math.max(0, -deltaHeight)).setDuration(DURATION_SLIDE_UP_MS),
                    ObjectAnimator.ofFloat(mNewInnerView, View.ALPHA, 1f)
                            .setDuration(DURATION_FADE_OUT_MS));
            return animator;
        }

        @Override
        void onAnimationEnd() {
            mFrontView.removeViewAt(0);
            InfoBarContainerLayout.this.setTranslationY(0f);
            mFrontItem.setControlsEnabled(true);
            announceForAccessibility(mFrontItem.getAccessibilityText());
        }

        @Override
        int getAnimationType() {
            return InfoBarAnimationListener.ANIMATION_TYPE_SWAP;
        }
    }

    /**
     * All the Items, in front to back order.
     * This list is updated immediately when addInfoBar(), removeInfoBar(), and swapInfoBar() are
     * called; so during animations, it does *not* match the currently visible views.
     */
    private final ArrayList<Item> mItems = new ArrayList<>();

    /**
     * The Item that is currently shown on the front (i.e. top) of the stack. Like mItems, this
     * value is not meaningful during animations.
     */
    private Item mFrontItem;

    /** The current animation, or null if no animation is happening currently. */
    private InfoBarAnimation mAnimation;

    private InfoBarAnimationListener mAnimationListener;

    /**
     * The height of back infobars, i.e. the distance between the top of the front infobar and the
     * top of the next infobar back.
     */
    private int mBackInfobarHeight;

    private void processPendingAnimations() {
        // If an animation is running, wait until it finishes before beginning the next animation.
        if (mAnimation != null) return;

        // The steps below are ordered to minimize movement during animations. In particular,
        // removals happen before additions or swaps, and changes are made to back infobars before
        // front infobars.

        int childCount = getChildCount();
        int desiredChildCount = Math.min(mItems.size(), MAX_STACK_DEPTH);
        boolean shouldRemoveFrontView = mFrontItem != null && !mItems.contains(mFrontItem);

        // First, check if we can remove any back infobars.
        if (childCount > desiredChildCount + 1
                || (childCount == desiredChildCount + 1 && !shouldRemoveFrontView)) {
            scheduleAnimation(new InfoBarDisappearingAnimation());
            return;
        }

        // Second, check if we should remove the front infobar.
        if (shouldRemoveFrontView) {
            // The second to front infobar, if any, will become the new front infobar.
            if (!mItems.isEmpty() && childCount >= 2) {
                mFrontItem = mItems.get(0);
                scheduleAnimation(new FrontInfoBarDisappearingAndRevealingAnimation(
                        mFrontItem.getView()));
                return;
            } else {
                mFrontItem = null;
                scheduleAnimation(new InfoBarDisappearingAnimation());
                return;
            }
        }

        // Third, run swap animation on front infobar if needed.
        if (mFrontItem != null && mItems.contains(mFrontItem)) {
            View frontInnerView = ((ViewGroup) getChildAt(0)).getChildAt(0);
            if (frontInnerView != mFrontItem.getView()) {
                scheduleAnimation(new FrontInfoBarSwapContentsAnimation(mFrontItem.getView()));
                return;
            }
        }

        // Fourth, check if we should add any infobars.
        if (childCount < desiredChildCount) {
            if (childCount == 0) {
                mFrontItem = mItems.get(0);
                scheduleAnimation(new FrontInfoBarAppearingAnimation(mFrontItem.getView()));
                return;
            } else {
                scheduleAnimation(new BackInfoBarAppearingAnimation());
                return;
            }
        }
    }

    private void scheduleAnimation(InfoBarAnimation animation) {
        mAnimation = animation;
        mAnimation.prepareAnimation();
        // Trigger a layout. onLayout() will call mAnimation.start().
        requestLayout();
    }

    private void updateLayoutParams() {
        // Stagger the top margins so the back infobars peek out a bit.
        int childCount = getChildCount();
        for (int i = 0; i < childCount; i++) {
            View child = getChildAt(i);
            LayoutParams lp = (LayoutParams) child.getLayoutParams();
            lp.topMargin = (childCount - 1 - i) * mBackInfobarHeight;
            child.setLayoutParams(lp);
        }
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);

        // Animations start after a layout has completed, at which point all views are guaranteed
        // to have valid sizes and positions.
        if (mAnimation != null && !mAnimation.isStarted()) {
            mAnimation.start();
        }
    }

    @Override
    protected int getChildDrawingOrder(int childCount, int i) {
        // Draw children from last to first. This allows us to order the children from front to
        // back, which simplifies the logic elsewhere in this class.
        return childCount - i - 1;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        // Trap any attempts to fiddle with the infobars while we're animating.
        return super.onInterceptTouchEvent(ev) || mAnimation != null
                || (mFrontItem != null && !mFrontItem.areControlsEnabled());
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        super.onTouchEvent(event);
        // Consume all touch events so they do not reach the ContentView.
        return true;
    }

    @Override
    public boolean onHoverEvent(MotionEvent event) {
        super.onHoverEvent(event);
        // Consume all hover events so they do not reach the ContentView. In touch exploration mode,
        // this prevents the user from interacting with the part of the ContentView behind the
        // infobars. http://crbug.com/430701
        return true;
    }
}

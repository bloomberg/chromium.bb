// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.base.ObserverList;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.banners.SwipableOverlayView;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.ArrayList;


/**
 * A container for all the infobars of a specific tab.
 * Note that infobars creation can be initiated from Java of from native code.
 * When initiated from native code, special code is needed to keep the Java and native infobar in
 * sync, see NativeInfoBar.
 */
public class InfoBarContainer extends SwipableOverlayView {
    private static final String TAG = "InfoBarContainer";

    /** Top margin, including the toolbar and tabstrip height and 48dp of web contents. */
    private static final int TOP_MARGIN_PHONE_DP = 104;
    private static final int TOP_MARGIN_TABLET_DP = 144;

    /** Whether or not the InfoBarContainer is allowed to hide when the user scrolls. */
    private static boolean sIsAllowedToAutoHide = true;

    /**
     * A listener for the InfoBar animations.
     */
    public interface InfoBarAnimationListener {
        public static final int ANIMATION_TYPE_SHOW = 0;
        public static final int ANIMATION_TYPE_SWAP = 1;
        public static final int ANIMATION_TYPE_HIDE = 2;

        /**
         * Notifies the subscriber when an animation is completed.
         */
        void notifyAnimationFinished(int animationType);
    }

    /**
     * An observer that is notified of changes to a {@link InfoBarContainer} object.
     */
    public interface InfoBarContainerObserver {
        /**
         * Called when an {@link InfoBar} is about to be added (before the animation).
         * @param container The notifying {@link InfoBarContainer}
         * @param infoBar An {@link InfoBar} being added
         * @param isFirst Whether the infobar container was empty
         */
        void onAddInfoBar(InfoBarContainer container, InfoBar infoBar, boolean isFirst);

        /**
         * Called when an {@link InfoBar} is about to be removed (before the animation).
         * @param container The notifying {@link InfoBarContainer}
         * @param infoBar An {@link InfoBar} being removed
         * @param isLast Whether the infobar container is going to be empty
         */
        void onRemoveInfoBar(InfoBarContainer container, InfoBar infoBar, boolean isLast);
    }

    private final InfoBarContainerLayout mLayout;

    /** Native InfoBarContainer pointer which will be set by nativeInit(). */
    private final long mNativeInfoBarContainer;

    private final Context mContext;

    /** The list of all InfoBars in this container, regardless of whether they've been shown yet. */
    private final ArrayList<InfoBar> mInfoBars = new ArrayList<InfoBar>();

    /** True when this container has been emptied and its native counterpart has been destroyed. */
    private boolean mDestroyed = false;

    /** The id of the tab associated with us. Set to Tab.INVALID_TAB_ID if no tab is associated. */
    private int mTabId;

    /** Parent view that contains the InfoBarContainerLayout. */
    private ViewGroup mParentView;

    private final ObserverList<InfoBarContainerObserver> mObservers =
            new ObserverList<InfoBarContainerObserver>();

    public InfoBarContainer(Context context, int tabId, ViewGroup parentView, Tab tab) {
        super(context, null);
        tab.addObserver(getTabObserver());
        setIsSwipable(false);

        // TODO(newt): move this workaround into the infobar views if/when they're scrollable.
        // Workaround for http://crbug.com/407149. See explanation in onMeasure() below.
        setVerticalScrollBarEnabled(false);

        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT, Gravity.BOTTOM);
        int topMarginDp = DeviceFormFactor.isTablet(context)
                ? TOP_MARGIN_TABLET_DP : TOP_MARGIN_PHONE_DP;
        lp.topMargin = Math.round(topMarginDp * getResources().getDisplayMetrics().density);
        setLayoutParams(lp);

        mContext = context;
        mTabId = tabId;
        mParentView = parentView;

        mLayout = new InfoBarContainerLayout(context);
        addView(mLayout);

        // Chromium's InfoBarContainer may add an InfoBar immediately during this initialization
        // call, so make sure everything in the InfoBarContainer is completely ready beforehand.
        mNativeInfoBarContainer = nativeInit();
    }

    /**
     * Adds an {@link InfoBarContainerObserver}.
     * @param observer The {@link InfoBarContainerObserver} to add.
     */
    public void addObserver(InfoBarContainerObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes a {@link InfoBarContainerObserver}.
     * @param observer The {@link InfoBarContainerObserver} to remove.
     */
    public void removeObserver(InfoBarContainerObserver observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void setContentViewCore(ContentViewCore contentViewCore) {
        super.setContentViewCore(contentViewCore);
        if (getContentViewCore() != null) {
            nativeSetWebContents(mNativeInfoBarContainer, contentViewCore.getWebContents());
        }
    }

    @VisibleForTesting
    public void setAnimationListener(InfoBarAnimationListener listener) {
        mLayout.setAnimationListener(listener);
    }

    /**
     * Returns true if any animations are pending or in progress.
     */
    @VisibleForTesting
    public boolean isAnimating() {
        return mLayout.isAnimating();
    }

    private void addToParentView() {
        super.addToParentView(mParentView);
    }

    /**
     * Called when the parent {@link android.view.ViewGroup} has changed for
     * this container.
     */
    public void onParentViewChanged(int tabId, ViewGroup parentView) {
        mTabId = tabId;
        mParentView = parentView;

        removeFromParentView();
        addToParentView();
    }

    @Override
    protected TabObserver createTabObserver() {
        return new SwipableOverlayViewTabObserver() {
            @Override
            public void onPageLoadStarted(Tab tab, String url) {
                onPageStarted();
            }
        };
    }

    /**
     * Adds an InfoBar to the view hierarchy.
     * @param infoBar InfoBar to add to the View hierarchy.
     */
    @CalledByNative
    public void addInfoBar(InfoBar infoBar) {
        assert !mDestroyed;
        if (infoBar == null) {
            return;
        }
        if (mInfoBars.contains(infoBar)) {
            assert false : "Trying to add an info bar that has already been added.";
            return;
        }
        addToParentView();

        // We notify observers immediately (before the animation starts).
        for (InfoBarContainerObserver observer : mObservers) {
            observer.onAddInfoBar(this, infoBar, mInfoBars.isEmpty());
        }

        // We add the infobar immediately to mInfoBars but we wait for the animation to end to
        // notify it's been added, as tests rely on this notification but expects the infobar view
        // to be available when they get the notification.
        mInfoBars.add(infoBar);
        infoBar.setContext(mContext);
        infoBar.setInfoBarContainer(this);
        infoBar.createView();

        mLayout.addInfoBar(infoBar);
    }

    /**
     * Notifies that an infobar's View ({@link InfoBar#getView}) has changed. If the infobar is
     * visible, a view swapping animation will be run.
     */
    public void notifyInfoBarViewChanged() {
        assert !mDestroyed;
        mLayout.notifyInfoBarViewChanged();
    }

    /**
     * Removes an InfoBar from the view hierarchy.
     * @param infoBar InfoBar to remove from the View hierarchy.
     */
    public void removeInfoBar(InfoBar infoBar) {
        assert !mDestroyed;

        if (!mInfoBars.remove(infoBar)) {
            assert false : "Trying to remove an InfoBar that is not in this container.";
            return;
        }

        // Notify observers immediately, before any animations begin.
        for (InfoBarContainerObserver observer : mObservers) {
            observer.onRemoveInfoBar(this, infoBar, mInfoBars.isEmpty());
        }

        mLayout.removeInfoBar(infoBar);
    }

    /**
     * @return True when this container has been emptied and its native counterpart has been
     *         destroyed.
     */
    public boolean hasBeenDestroyed() {
        return mDestroyed;
    }

    // Called by the tab when it has started loading a new page.
    public void onPageStarted() {
        ArrayList<InfoBar> barsToRemove = new ArrayList<>();

        for (InfoBar infoBar : mInfoBars) {
            if (infoBar.shouldExpire()) {
                barsToRemove.add(infoBar);
            }
        }

        for (InfoBar infoBar : barsToRemove) {
            infoBar.dismissJavaOnlyInfoBar();
        }
    }

    public void destroy() {
        mDestroyed = true;
        if (mNativeInfoBarContainer != 0) {
            nativeDestroy(mNativeInfoBarContainer);
        }
    }

    /**
     * @return all of the InfoBars held in this container.
     */
    @VisibleForTesting
    public ArrayList<InfoBar> getInfoBars() {
        return mInfoBars;
    }

    /**
     * Sets whether the InfoBarContainer is allowed to auto-hide when the user scrolls the page.
     * Expected to be called when Touch Exploration is enabled.
     * @param isAllowed Whether auto-hiding is allowed.
     */
    public static void setIsAllowedToAutoHide(boolean isAllowed) {
        sIsAllowedToAutoHide = isAllowed;
    }

    @Override
    protected boolean isAllowedToAutoHide() {
        return sIsAllowedToAutoHide;
    }

    @Override
    protected void onViewSwipedAway() {
        assert false;
    }

    @Override
    protected void onViewClicked() {
        assert false;
    }

    @Override
    protected void onViewPressed(MotionEvent event) {
        assert false;
    }

    private native long nativeInit();
    private native void nativeSetWebContents(
            long nativeInfoBarContainerAndroid, WebContents webContents);
    private native void nativeDestroy(long nativeInfoBarContainerAndroid);
}

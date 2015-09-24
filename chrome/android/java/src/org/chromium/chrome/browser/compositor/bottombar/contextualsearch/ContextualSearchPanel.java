// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.content.Context;
import android.os.Handler;
import android.view.View.MeasureSpec;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchContentController;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler;
import org.chromium.components.navigation_interception.InterceptNavigationDelegate;
import org.chromium.components.navigation_interception.NavigationParams;
import org.chromium.components.web_contents_delegate_android.WebContentsDelegateAndroid;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.ContentViewClient;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;

/**
 * Controls the Contextual Search Panel.
 */
public class ContextualSearchPanel extends ContextualSearchPanelAnimation
        implements ContextualSearchPanelDelegate, ContextualSearchContentController {

    /**
     * State of the Contextual Search Panel.
     */
    public static enum PanelState {
        UNDEFINED,
        CLOSED,
        PEEKED,
        EXPANDED,
        MAXIMIZED;
    }

    /**
     * The reason for a change in the Contextual Search Panel's state.
     */
    public static enum StateChangeReason {
        UNKNOWN,
        RESET,
        BACK_PRESS,
        TEXT_SELECT_TAP,
        TEXT_SELECT_LONG_PRESS,
        INVALID_SELECTION,
        CLEARED_SELECTION,
        BASE_PAGE_TAP,
        BASE_PAGE_SCROLL,
        SEARCH_BAR_TAP,
        SERP_NAVIGATION,
        TAB_PROMOTION,
        CLICK,
        SWIPE,
        FLING,
        OPTIN,
        OPTOUT,
        CLOSE_BUTTON;
    }

    // The animation duration of a URL being promoted to a tab when triggered by an
    // intercept navigation. This is faster than the standard tab promotion animation
    // so that it completes before the navigation.
    private static final long INTERCEPT_NAVIGATION_PROMOTION_ANIMATION_DURATION_MS = 40;

    /**
     * The ContentViewCore that this panel will display.
     */
    private ContentViewCore mContentViewCore;

    /**
     * The pointer to the native version of this class.
     */
    private long mNativeContextualSearchPanelPtr;

    /**
     * Used for progress bar handling.
     */
    private final WebContentsDelegateAndroid mWebContentsDelegate;

    /**
     * The activity this panel is in.
     */
    private ChromeActivity mActivity;

    /**
     * The window android for the above activity.
     */
    private WindowAndroid mWindowAndroid;

    /**
     * Observes the ContentViewCore for this panel.
     */
    private WebContentsObserver mWebContentsObserver;

    /**
     * Used to detect if a URL was loaded in the ContentViewCore.
     */
    private boolean mDidLoadAnyUrl;

    /**
     * Used to track if the ContentViewCore is showing.
     */
    private boolean mIsContentViewShowing;

    /**
     * This is primarily used for injecting test functionaluty into the panel for creating and
     * destroying ContentViewCores.
     */
    private ContextualSearchContentController mContentController;

    // http://crbug.com/522266 : An instance of InterceptNavigationDelegateImpl should be kept in
    // java layer. Otherwise, the instance could be garbage-collected unexpectedly.
    private InterceptNavigationDelegateImpl mInterceptNavigationDelegate;

    /**
     * The delay after which the hide progress will be hidden.
     */
    private static final long HIDE_PROGRESS_BAR_DELAY = 1000 / 60 * 4;

    /**
     * The initial height of the Contextual Search Panel.
     */
    private float mInitialPanelHeight;

    /**
     * Whether the Panel should be promoted to a new tab after being maximized.
     */
    private boolean mShouldPromoteToTabAfterMaximizing;

    /**
     * Whether a touch gesture has been detected.
     */
    private boolean mHasDetectedTouchGesture;

    /**
     * Whether the search content view has been touched.
     */
    private boolean mHasSearchContentViewBeenTouched;

    /**
     * The {@link ContextualSearchPanelHost} used to communicate with the supported layout.
     */
    private ContextualSearchPanelHost mSearchPanelHost;

    // ============================================================================================
    // Constructor
    // ============================================================================================

    /**
     * @param context The current Android {@link Context}.
     * @param updateHost The {@link LayoutUpdateHost} used to request updates in the Layout.
     */
    public ContextualSearchPanel(Context context, LayoutUpdateHost updateHost) {
        super(context, updateHost);
        nativeInit();

        // TODO(mdjones): The following is for testing purposes, refactor the need for this out.
        mContentController = this;

        mWebContentsDelegate = new WebContentsDelegateAndroid() {
            @Override
            public void onLoadStarted() {
                super.onLoadStarted();
                setProgressBarCompletion(0);
                setProgressBarVisible(true);
                requestUpdate();
            }

            @Override
            public void onLoadStopped() {
                super.onLoadStopped();
                // Hides the Progress Bar after a delay to make sure it is rendered for at least
                // a few frames, otherwise its completion won't be visually noticeable.
                new Handler().postDelayed(new Runnable() {
                    @Override
                    public void run() {
                        setProgressBarVisible(false);
                        requestUpdate();
                    }
                }, HIDE_PROGRESS_BAR_DELAY);
            }

            @Override
            public void onLoadProgressChanged(int progress) {
                super.onLoadProgressChanged(progress);
                setProgressBarCompletion(progress);
                requestUpdate();
            }
        };
    }

    // ============================================================================================
    // Layout Integration
    // ============================================================================================

    /**
     * Sets the {@ContextualSearchPanelHost} used to communicate with the supported layout.
     * @param host The {@ContextualSearchPanelHost}.
     */
    public void setHost(ContextualSearchPanelHost host) {
        mSearchPanelHost = host;
    }

    // ============================================================================================
    // Contextual Search Manager Integration
    // ============================================================================================

    @Override
    public void setPreferenceState(boolean enabled) {
        if (getManagementDelegate() != null) {
            getManagementDelegate().setPreferenceState(enabled);
        }
    }

    @Override
    protected boolean isPromoAvailable() {
        return getManagementDelegate() != null && getManagementDelegate().isPromoAvailable();
    }

    @Override
    public void onPromoButtonClick(boolean accepted) {
        super.onPromoButtonClick(accepted);
        getManagementDelegate().logPromoOutcome();
        setIsPromoActive(false);
    }

    @Override
    protected void onClose(StateChangeReason reason) {
        mContentController.destroyContentView();
        getManagementDelegate().onCloseContextualSearch(reason);
    }

    // ============================================================================================
    // Generic Event Handling
    // ============================================================================================

    /**
     * Handles the beginning of the swipe gesture.
     */
    public void handleSwipeStart() {
        if (animationIsRunning()) {
            cancelHeightAnimation();
        }

        mHasDetectedTouchGesture = false;
        mInitialPanelHeight = getHeight();
    }

    /**
     * Handles the movement of the swipe gesture.
     *
     * @param ty The movement's total displacement in dps.
     */
    public void handleSwipeMove(float ty) {
        if (ty > 0 && getPanelState() == PanelState.MAXIMIZED) {
            // Resets the Search Content View scroll position when swiping the Panel down
            // after being maximized.
            resetSearchContentViewScroll();
        }

        // Negative ty value means an upward movement so subtracting ty means expanding the panel.
        setClampedPanelHeight(mInitialPanelHeight - ty);
        requestUpdate();
    }

    /**
     * Handles the end of the swipe gesture.
     */
    public void handleSwipeEnd() {
        // This method will be called after handleFling() and handleClick()
        // methods because we also need to track down the onUpOrCancel()
        // action from the Layout. Therefore the animation to the nearest
        // PanelState should only happen when no other gesture has been
        // detected.
        if (!mHasDetectedTouchGesture) {
            mHasDetectedTouchGesture = true;
            animateToNearestState();
        }
    }

    /**
     * Handles the fling gesture.
     *
     * @param velocity The velocity of the gesture in dps per second.
     */
    public void handleFling(float velocity) {
        mHasDetectedTouchGesture = true;
        animateToProjectedState(velocity);
    }

    /**
     * Handles the click gesture.
     *
     * @param time The timestamp of the gesture.
     * @param x The x coordinate of the gesture.
     * @param y The y coordinate of the gesture.
     */
    public void handleClick(long time, float x, float y) {
        mHasDetectedTouchGesture = true;
        if (isCoordinateInsideBasePage(x, y)) {
            closePanel(StateChangeReason.BASE_PAGE_TAP, true);
        } else if (isCoordinateInsideSearchBar(x, y)) {
            if (isPeeking()) {
                if (getManagementDelegate().isRunningInCompatibilityMode()) {
                    getManagementDelegate().openResolvedSearchUrlInNewTab();
                } else {
                    if (isFullscreenSizePanel()) {
                        expandPanel(StateChangeReason.SEARCH_BAR_TAP);
                    } else {
                        maximizePanel(StateChangeReason.SEARCH_BAR_TAP);
                    }
                }
            } else if (isExpanded()) {
                peekPanel(StateChangeReason.SEARCH_BAR_TAP);
            } else if (isMaximized()) {
                if (mSearchPanelFeatures.isSearchTermRefiningAvailable()) {
                    getManagementDelegate().promoteToTab();
                }
                if (mSearchPanelFeatures.isCloseButtonAvailable()
                        && isCoordinateInsideCloseButton(x, y)) {
                    closePanel(StateChangeReason.CLOSE_BUTTON, true);
                }
            }
        }
    }

    // ============================================================================================
    // Gesture Event helpers
    // ============================================================================================

    /**
     * @param x The x coordinate in dp.
     * @param y The y coordinate in dp.
     * @return Whether the given coordinate is inside the Search Panel area.
     */
    private boolean isCoordinateInsideSearchPanel(float x, float y) {
        return y >= getOffsetY() && y <= (getOffsetY() + getHeight())
                &&  x >= getOffsetX() && x <= (getOffsetX() + getWidth());
    }

    /**
     * @param x The x coordinate in dp.
     * @param y The y coordinate in dp.
     * @return Whether the given coordinate is inside the Base Page area.
     */
    private boolean isCoordinateInsideBasePage(float x, float y) {
        return !isCoordinateInsideSearchPanel(x, y);
    }

    /**
     * @param x The x coordinate in dp.
     * @param y The y coordinate in dp.
     * @return Whether the given coordinate is inside the Search Bar area.
     */
    public boolean isCoordinateInsideSearchBar(float x, float y) {
        return isCoordinateInsideSearchPanel(x, y)
                && y >= getOffsetY() && y <= (getOffsetY() + getSearchBarHeight());
    }

    /**
     * @param x The x coordinate in dp.
     * @param y The y coordinate in dp.
     * @return Whether the given coordinate is inside the Search Content View area.
     */
    public boolean isCoordinateInsideSearchContentView(float x, float y) {
        return isCoordinateInsideSearchPanel(x, y)
                && y > getSearchContentViewOffsetY();
    }

    /**
     * @return The horizontal offset of the Search Content View in dp.
     */
    public float getSearchContentViewOffsetX() {
        return getOffsetX();
    }

    /**
     * @return The vertical offset of the Search Content View in dp.
     */
    public float getSearchContentViewOffsetY() {
        return getOffsetY() + getSearchBarHeight() + getPromoHeight();
    }

    /**
     * @param x The x coordinate in dp.
     * @param y The y coordinate in dp.
     * @return Whether the given |x| |y| coordinate is inside the close button.
     */
    private boolean isCoordinateInsideCloseButton(float x, float y) {
        boolean isInY = y >= getCloseIconY() && y <= (getCloseIconY() + getCloseIconDimension());
        boolean isInX = x >= getCloseIconX() && x <= (getCloseIconX() + getCloseIconDimension());
        return isInY && isInX;
    }

    /**
     * @return Whether the Panel is in its expanded state.
     */
    protected boolean isExpanded() {
        return doesPanelHeightMatchState(PanelState.EXPANDED);
    }

    /**
     * Acknowledges that there was a touch in the search content view, though no immediate action
     * needs to be taken.
     */
    public void onTouchSearchContentViewAck() {
        mHasSearchContentViewBeenTouched = true;
    }

    // ============================================================================================
    // Animation Handling
    // ============================================================================================

    @Override
    protected void onAnimationFinished() {
        super.onAnimationFinished();

        if (shouldHideContextualSearchLayout()) {
            if (mSearchPanelHost != null) {
                mSearchPanelHost.hideLayout(false);
            }
        }

        if (mShouldPromoteToTabAfterMaximizing && getPanelState() == PanelState.MAXIMIZED) {
            mShouldPromoteToTabAfterMaximizing = false;
            getManagementDelegate().promoteToTab();
        }
    }

    /**
    * Whether the Contextual Search Layout should be hidden.
    *
    * @return Whether the Contextual Search Layout should be hidden.
    */
    private boolean shouldHideContextualSearchLayout() {
        final PanelState state = getPanelState();

        return (state == PanelState.PEEKED || state == PanelState.CLOSED)
                && getHeight() == getPanelHeightFromState(state);
    }

    // ============================================================================================
    // ContextualSearchContentController
    // ============================================================================================

    @Override
    public void createNewContentView() {
        // If the ContentViewCore has already been created, but never used,
        // then there's no need to create a new one.
        if (mContentViewCore != null && !mDidLoadAnyUrl) {
            return;
        }

        mContentController.destroyContentView();
        mContentViewCore = new ContentViewCore(mActivity);

        // Adds a ContentViewClient to override the default fullscreen size.
        if (!isFullscreenSizePanel()) {
            mContentViewCore.setContentViewClient(new ContentViewClient() {
                @Override
                public int getDesiredWidthMeasureSpec() {
                    return MeasureSpec.makeMeasureSpec(
                            getSearchContentViewWidthPx(),
                            MeasureSpec.EXACTLY);
                }

                @Override
                public int getDesiredHeightMeasureSpec() {
                    return MeasureSpec.makeMeasureSpec(
                            getSearchContentViewHeightPx(),
                            MeasureSpec.EXACTLY);
                }
            });
        }

        ContentView cv = ContentView.createContentView(mActivity, mContentViewCore);
        // Creates an initially hidden WebContents which gets shown when the panel is opened.
        mContentViewCore.initialize(cv, cv,
                WebContentsFactory.createWebContents(false, true), mWindowAndroid);

        // Transfers the ownership of the WebContents to the native ContextualSearchManager.
        nativeSetWebContents(mNativeContextualSearchPanelPtr, mContentViewCore,
                mWebContentsDelegate);

        mWebContentsObserver =
                new WebContentsObserver(mContentViewCore.getWebContents()) {
                    @Override
                    public void didStartLoading(String url) {
                        getManagementDelegate().onStartedLoading();
                    }

                    @Override
                    public void didStartProvisionalLoadForFrame(long frameId, long parentFrameId,
                            boolean isMainFrame, String validatedUrl, boolean isErrorPage,
                            boolean isIframeSrcdoc) {
                        if (isMainFrame) getManagementDelegate().onExternalNavigation(validatedUrl);
                    }

                    @Override
                    public void didNavigateMainFrame(String url, String baseUrl,
                            boolean isNavigationToDifferentPage, boolean isNavigationInPage,
                            int httpResultCode) {
                        getManagementDelegate().handleDidNavigateMainFrame(url, httpResultCode);
                        mDidLoadAnyUrl = false;
                    }

                    @Override
                    public void didFinishLoad(long frameId, String validatedUrl,
                            boolean isMainFrame) {
                        getManagementDelegate().onSearchResultsLoaded();
                    }
                };

        mInterceptNavigationDelegate = new InterceptNavigationDelegateImpl();
        nativeSetInterceptNavigationDelegate(mNativeContextualSearchPanelPtr,
                mInterceptNavigationDelegate, mContentViewCore.getWebContents());

        getManagementDelegate().onContentViewCreated(mContentViewCore);
    }

    @Override
    public void destroyContentView() {
        // TODO(mdjones): This should not be a public API.
        if (mContentViewCore != null) {
            nativeDestroyWebContents(mNativeContextualSearchPanelPtr);
            mContentViewCore.getWebContents().destroy();
            mContentViewCore.destroy();
            mContentViewCore = null;
            if (mWebContentsObserver != null) {
                mWebContentsObserver.destroy();
                mWebContentsObserver = null;
            }

            getManagementDelegate().onContentViewDestroyed();
        }

        // This should be called last here. The setSearchContentViewVisibility method
        // will change the visibility the SearchContentView but also set the value of the
        // internal property mIsSearchContentViewShowing. If we call this after deleting
        // the SearchContentView, it will be faster, because only the internal property
        // will be changed, since there will be no need to change the visibility of the
        // SearchContentView.
        //
        // Also, this should be called outside the block above because tests will not
        // create a ContentView, therefore if this call is placed inside the block,
        // it will not be called on tests, which will cause some tests to fail.
        setSearchContentViewVisibility(false);
    }

    @Override
    public void loadUrl(String url) {
        createNewPanelContentView();

        if (mContentViewCore != null && mContentViewCore.getWebContents() != null) {
            mDidLoadAnyUrl = true;
            mContentViewCore.getWebContents().getNavigationController().loadUrl(
                    new LoadUrlParams(url));
        }
    }

    public void resetSearchContentViewScroll() {
        if (mContentViewCore != null) {
            mContentViewCore.scrollTo(0, 0);
        }
    }

    public float getSearchContentViewVerticalScroll() {
        return mContentViewCore != null
                ? mContentViewCore.computeVerticalScrollOffset() : -1.f;
    }

    /**
     * Sets the visibility of the Search Content View.
     * @param isVisible True to make it visible.
     */
    public void setSearchContentViewVisibility(boolean isVisible) {
        if (mIsContentViewShowing == isVisible) return;

        mIsContentViewShowing = isVisible;
        getManagementDelegate().onContentViewVisibilityChanged(isVisible);

        if (isVisible) {
            // The CVC is created with the search request, but if none was made we'll need
            // one in order to display an empty panel.
            if (mContentViewCore == null) {
                createNewPanelContentView();
            }
            if (mContentViewCore != null) mContentViewCore.onShow();
            setWasSearchContentViewSeen();
        } else {
            if (mContentViewCore != null) mContentViewCore.onHide();
        }
    }

    // ============================================================================================
    // InterceptNavigationDelegateImpl
    // ============================================================================================

    // Used to intercept intent navigations.
    // TODO(jeremycho): Consider creating a Tab with the Panel's ContentViewCore,
    // which would also handle functionality like long-press-to-paste.
    private class InterceptNavigationDelegateImpl implements InterceptNavigationDelegate {
        final ExternalNavigationHandler mExternalNavHandler = new ExternalNavigationHandler(
                mActivity);
        @Override
        public boolean shouldIgnoreNavigation(NavigationParams navigationParams) {
            // TODO(mdjones): Rather than passing the two navigation params, instead consider
            // passing a boolean to make this API simpler.
            return getManagementDelegate().shouldInterceptNavigation(mExternalNavHandler,
                    navigationParams);
        }
    }

    // ============================================================================================
    // Panel Delegate
    // ============================================================================================

    @Override
    public boolean isFullscreenSizePanel() {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        return super.isFullscreenSizePanel();
    }

    @Override
    public boolean isShowing() {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        return super.isShowing();
    }

    @Override
    public boolean isPeeking() {
        return doesPanelHeightMatchState(PanelState.PEEKED);
    }

    @Override
    public int getMaximumWidthPx() {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        return super.getMaximumWidthPx();
    }

    @Override
    public int getMaximumHeightPx() {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        return super.getMaximumHeightPx();
    }

    @Override
    public int getSearchContentViewWidthPx() {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        return super.getSearchContentViewWidthPx();
    }

    @Override
    public int getSearchContentViewHeightPx() {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        return super.getSearchContentViewHeightPx();
    }

    @Override
    public void maximizePanelThenPromoteToTab(StateChangeReason reason) {
        mShouldPromoteToTabAfterMaximizing = true;
        maximizePanel(reason);
    }

    @Override
    public void maximizePanelThenPromoteToTab(StateChangeReason reason, long duration) {
        mShouldPromoteToTabAfterMaximizing = true;
        animatePanelToState(PanelState.MAXIMIZED, reason, duration);
    }

    @Override
    public void peekPanel(StateChangeReason reason) {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        super.peekPanel(reason);

        if (getPanelState() == PanelState.CLOSED || getPanelState() == PanelState.PEEKED) {
            mHasSearchContentViewBeenTouched = false;
        }
    }

    @Override
    public void closePanel(StateChangeReason reason, boolean animate) {
        // If the close action is animated, the Layout will be hidden when
        // the animation is finished, so we should only hide the Layout
        // here when not animating.
        if (!animate && mSearchPanelHost != null) {
            mSearchPanelHost.hideLayout(true);
        }
        mHasSearchContentViewBeenTouched = false;

        super.closePanel(reason, animate);
    }

    @Override
    public void updateBasePageSelectionYPx(float y) {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        super.updateBasePageSelectionYPx(y);
    }

    @Override
    public PanelState getPanelState() {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        return super.getPanelState();
    }

    @Override
    public void setDidSearchInvolvePromo() {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        super.setDidSearchInvolvePromo();
    }

    public void setWasSearchContentViewSeen() {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        super.setWasSearchContentViewSeen();
    }

    @Override
    public void setIsPromoActive(boolean shown) {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        super.setIsPromoActive(shown);
    }

    @Override
    public boolean didTouchSearchContentView() {
        return mHasSearchContentViewBeenTouched;
    }

    @Override
    public void onSearchResultsLoaded(boolean wasPrefetch) {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        super.onSearchResultsLoaded(wasPrefetch);
    }

    @Override
    public BottomBarTextControl getBottomBarTextControl() {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        return super.getBottomBarTextControl();
    }

    @Override
    public boolean shouldAnimatePanelCloseOnPromoteToTab() {
        return mSearchPanelFeatures.shouldAnimatePanelCloseOnPromoteToTab();
    }

    @Override
    public void displaySearchTerm(String searchTerm) {
        cancelSearchTermResolutionAnimation();
        getBottomBarTextControl().setSearchTerm(searchTerm);
        resetBottomBarSearchTermVisibility();
    }

    @Override
    public void displaySearchContext(String selection, String start, String end) {
        cancelSearchTermResolutionAnimation();
        getBottomBarTextControl().setSearchContext(selection, start, end);
        resetBottomBarSearchContextVisibility();
    }

    @Override
    public void onSearchTermResolutionResponse(String searchTerm) {
        getBottomBarTextControl().setSearchTerm(searchTerm);
        animateSearchTermResolution();
    }

    @Override
    public boolean isContentViewShowing() {
        return mIsContentViewShowing;
    }

    @Override
    public void setChromeActivity(ChromeActivity activity) {
        mActivity = activity;
        mWindowAndroid = mActivity.getWindowAndroid();
    }

    @Override
    public void createNewPanelContentView() {
        mContentController.createNewContentView();
    }

    @Override
    public void loadUrlInPanel(String url) {
        mContentController.loadUrl(url);
    }

    @Override
    public void setContentController(ContextualSearchContentController controller) {
        mContentController = controller;
    }

    @VisibleForTesting
    @Override
    public ContextualSearchContentController getContentController() {
        return this;
    }

    @Override
    public boolean didLoadAnyUrl() {
        return mDidLoadAnyUrl;
    }

    @Override
    public void updateTopControlState() {
        if (isFullscreenSizePanel()) {
            // Consider the ContentView height to be fullscreen, and inform the system that
            // the Toolbar is always visible (from the Compositor's perspective), even though
            // the Toolbar and Base Page might be offset outside the screen. This means the
            // renderer will consider the ContentView height to be the fullscreen height
            // minus the Toolbar height.
            //
            // This is necessary to fix the bugs: crbug.com/510205 and crbug.com/510206
            mContentViewCore.getWebContents().updateTopControlsState(false, true, false);
        } else {
            mContentViewCore.getWebContents().updateTopControlsState(true, false, false);
        }
    }

    // ============================================================================================
    // Methods for managing this panel's ContentViewCore.
    // ============================================================================================

    @CalledByNative
    public void clearNativePanelContentPtr() {
        assert mNativeContextualSearchPanelPtr != 0;
        mNativeContextualSearchPanelPtr = 0;
    }

    @CalledByNative
    public void setNativePanelContentPtr(long nativePtr) {
        assert mNativeContextualSearchPanelPtr == 0;
        mNativeContextualSearchPanelPtr = nativePtr;
    }

    @Override
    public ContentViewCore getContentViewCore() {
        return mContentViewCore;
    }

    @Override
    public void removeLastHistoryEntry(String historyUrl, long urlTimeMs) {
        nativeRemoveLastHistoryEntry(mNativeContextualSearchPanelPtr, historyUrl, urlTimeMs);
    }

    @VisibleForTesting
    public void destroy() {
        nativeDestroy(mNativeContextualSearchPanelPtr);
    }

    // Native calls.
    protected native long nativeInit();
    private native void nativeDestroy(long nativeContextualSearchPanel);
    private native void nativeRemoveLastHistoryEntry(
            long nativeContextualSearchPanel, String historyUrl, long urlTimeMs);
    private native void nativeSetWebContents(long nativeContextualSearchPanel,
            ContentViewCore contentViewCore, WebContentsDelegateAndroid delegate);
    private native void nativeDestroyWebContents(long nativeContextualSearchPanel);
    private native void nativeSetInterceptNavigationDelegate(long nativeContextualSearchPanel,
            InterceptNavigationDelegate delegate, WebContents webContents);
}

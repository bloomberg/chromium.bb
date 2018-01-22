// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.animation.ValueAnimator;
import android.content.res.Resources;
import android.support.annotation.Nullable;
import android.support.v4.view.ViewCompat;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.ViewGroup;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.Interpolator;
import android.widget.FrameLayout;

import org.chromium.base.CollectionUtil;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.ContextMenuManager.TouchEnabledDelegate;
import org.chromium.chrome.browser.ntp.LogoBridge.Logo;
import org.chromium.chrome.browser.ntp.LogoBridge.LogoObserver;
import org.chromium.chrome.browser.ntp.LogoDelegateImpl;
import org.chromium.chrome.browser.ntp.LogoView;
import org.chromium.chrome.browser.ntp.cards.NewTabPageAdapter;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.omnibox.LocationBarPhone;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;
import org.chromium.chrome.browser.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.BottomToolbarPhone;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.chrome.browser.widget.LoadingView;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.StateChangeReason;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContentController;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetNewTabController;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;
import org.chromium.content.browser.BrowserStartupController;

import java.util.List;

/**
 * Provides content to be displayed inside of the Home tab of bottom sheet.
 */
public class SuggestionsBottomSheetContent implements BottomSheet.BottomSheetContent,
                                                      BottomSheetNewTabController.Observer,
                                                      TemplateUrlServiceObserver,
                                                      UrlFocusChangeListener {

    private static final long ANIMATION_DURATION_MS = 150L;

    private final View mView;
    private final SuggestionsRecyclerView mRecyclerView;
    private final ChromeActivity mActivity;
    private final BottomSheet mSheet;
    private final LocationBarPhone mLocationBar;
    private final BottomToolbarPhone mToolbarView;
    private final ViewGroup mControlContainerView;
    private final View mToolbarPullHandle;
    private final View mToolbarShadow;

    private NewTabPageAdapter mAdapter;
    private ContextMenuManager mContextMenuManager;
    private SuggestionsUiDelegateImpl mSuggestionsUiDelegate;
    private TileGroup.Delegate mTileGroupDelegate;
    private SuggestionsSheetVisibilityChangeObserver mBottomSheetObserver;
    private LogoView mLogoView;
    private LogoDelegateImpl mLogoDelegate;

    @Nullable
    private SuggestionsCarousel mSuggestionsCarousel;
    @Nullable
    private ContextualSuggestionsSection mContextualSuggestions;

    private boolean mNewTabShown;
    private boolean mSuggestionsInitialized;
    private boolean mIsDestroyed;
    private boolean mSearchProviderHasLogo = true;
    private float mLastSheetHeightFraction = 1f;

    /**
     * The transition fraction for hiding the logo: 0 means the logo is fully visible, 1 means it is
     * fully invisible and the toolbar is at the top. When the URL focus animation is running, that
     * determines the logo movement. Otherwise, the logo moves at the same speed as the scrolling of
     * the RecyclerView.
     */
    private float mTransitionFraction = 1f;
    private float mTransitionFractionBeforeAnimation = 1f;

    /** The toolbar height in pixels. */
    private final int mToolbarHeight;

    /** The maximum vertical toolbar offset in pixels. */
    private final float mMaxToolbarOffset;
    private ValueAnimator mAnimator;
    private final Interpolator mInterpolator = new DecelerateInterpolator();

    /**
     * Whether {@code mView} is currently attached to the window. This is used in place of
     * {@link View#isAttachedToWindow()} to support older versions of Android (KitKat & JellyBean).
     */
    private boolean mIsAttachedToWindow;

    public SuggestionsBottomSheetContent(final ChromeActivity activity, final BottomSheet sheet,
            TabModelSelector tabModelSelector, SnackbarManager snackbarManager) {
        mActivity = activity;
        mSheet = sheet;

        mView = LayoutInflater.from(activity).inflate(
                R.layout.suggestions_bottom_sheet_content, null);
        Resources resources = mView.getResources();
        int backgroundColor = SuggestionsConfig.getBackgroundColor(resources);
        mView.setBackgroundColor(backgroundColor);
        mRecyclerView = mView.findViewById(R.id.recycler_view);
        mRecyclerView.setBackgroundColor(backgroundColor);
        mToolbarHeight = resources.getDimensionPixelSize(R.dimen.bottom_control_container_height);
        mMaxToolbarOffset = resources.getDimensionPixelSize(R.dimen.ntp_logo_height)
                + resources.getDimensionPixelSize(R.dimen.ntp_logo_margin_top_modern)
                + resources.getDimensionPixelSize(R.dimen.ntp_logo_margin_bottom_modern)
                - mToolbarHeight;

        LoadingView loadingView = mView.findViewById(R.id.loading_view);

        mLocationBar = sheet.findViewById(R.id.location_bar);
        View.OnTouchListener touchListener = (View view, MotionEvent motionEvent) -> {
            if (mLocationBar != null && mLocationBar.isUrlBarFocused()) {
                mLocationBar.setUrlBarFocus(false);
            }

            // Never intercept the touch event.
            return false;
        };
        mView.setOnTouchListener(touchListener);
        mRecyclerView.setOnTouchListener(touchListener);

        mControlContainerView = (ViewGroup) activity.findViewById(R.id.control_container);
        mToolbarView = (BottomToolbarPhone) activity.findViewById(R.id.toolbar);
        mToolbarPullHandle = activity.findViewById(R.id.toolbar_handle);
        mToolbarShadow = activity.findViewById(R.id.bottom_toolbar_shadow);

        mLocationBar.addUrlFocusChangeListener(this);

        if (mActivity.didFinishNativeInitialization()) {
            loadingView.setVisibility(View.GONE);
            initializeWithNative(tabModelSelector, snackbarManager);
        } else {
            mRecyclerView.setVisibility(View.GONE);
            loadingView.showLoadingUI();
            // Only add a StartupCompletedObserver if native is not initialized to avoid
            // #initializeWithNative() being called twice.
            BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                    .addStartupCompletedObserver(new BrowserStartupController.StartupCallback() {
                        @Override
                        public void onSuccess(boolean alreadyStarted) {
                            // If this is destroyed before native initialization is finished, don't
                            // do anything. Otherwise this will be initialized based on out-of-date
                            // #mSheet and #mActivity, which causes a crash.
                            // See https://crbug.com/804296.
                            if (mIsDestroyed) return;

                            mRecyclerView.setVisibility(View.VISIBLE);
                            loadingView.hideLoadingUI();
                            initializeWithNative(tabModelSelector, snackbarManager);

                            // Update #mIsAttachedToWindow and #mNewTabShown since observers are
                            // added after native is initialized.
                            mIsAttachedToWindow = ViewCompat.isAttachedToWindow(mView);
                            mNewTabShown = mSheet.getNewTabController().isShowingNewTabUi();
                            updateLogoVisibility();
                            updateLogoTransition();
                        }

                        @Override
                        public void onFailure() {}
                    });
        }
    }

    private void initializeWithNative(
            TabModelSelector tabModelSelector, SnackbarManager snackbarManager) {
        assert !mSuggestionsInitialized;
        mSuggestionsInitialized = true;

        SuggestionsDependencyFactory depsFactory = SuggestionsDependencyFactory.getInstance();
        Profile profile = Profile.getLastUsedProfile();
        SuggestionsNavigationDelegate navigationDelegate =
                new SuggestionsNavigationDelegateImpl(mActivity, profile, mSheet, tabModelSelector);

        mTileGroupDelegate =
                new TileGroupDelegateImpl(mActivity, profile, navigationDelegate, snackbarManager);
        mSuggestionsUiDelegate = new SuggestionsUiDelegateImpl(
                depsFactory.createSuggestionSource(profile), depsFactory.createEventReporter(),
                navigationDelegate, profile, mSheet, mActivity.getReferencePool(), snackbarManager);

        TouchEnabledDelegate touchEnabledDelegate = mActivity.getBottomSheet()::setTouchEnabled;
        mContextMenuManager = new ContextMenuManager(
                navigationDelegate, touchEnabledDelegate, mActivity::closeContextMenu);
        mActivity.getWindowAndroid().addContextMenuCloseListener(mContextMenuManager);
        mSuggestionsUiDelegate.addDestructionObserver(() -> {
            mActivity.getWindowAndroid().removeContextMenuCloseListener(mContextMenuManager);
        });

        UiConfig uiConfig = new UiConfig(mRecyclerView);
        mRecyclerView.init(uiConfig, mContextMenuManager);

        OfflinePageBridge offlinePageBridge = depsFactory.getOfflinePageBridge(profile);

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_CAROUSEL)) {
            mSuggestionsCarousel = new SuggestionsCarousel(
                    uiConfig, mSuggestionsUiDelegate, mContextMenuManager, offlinePageBridge);
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_ABOVE_ARTICLES)) {
            mContextualSuggestions = new ContextualSuggestionsSection(mSuggestionsUiDelegate,
                    offlinePageBridge, mActivity, mActivity.getTabModelSelector());
        }

        // Inflate the logo in a container so its layout attributes are applied, then take it out.
        FrameLayout logoContainer = (FrameLayout) LayoutInflater.from(mActivity).inflate(
                R.layout.suggestions_bottom_sheet_logo, null);
        mLogoView = logoContainer.findViewById(R.id.search_provider_logo);
        logoContainer.removeView(mLogoView);

        mAdapter = new NewTabPageAdapter(mSuggestionsUiDelegate,
                /* aboveTheFoldView = */ null, mLogoView, uiConfig, offlinePageBridge,
                mContextMenuManager, mTileGroupDelegate, mSuggestionsCarousel,
                mContextualSuggestions);

        mBottomSheetObserver = new SuggestionsSheetVisibilityChangeObserver(this, mActivity) {
            @Override
            public void onContentShown(boolean isFirstShown) {
                // TODO(dgn): Temporary workaround to trigger an event in the backend when the
                // sheet is opened following inactivity. See https://crbug.com/760974. Should be
                // moved back to the "new opening of the sheet" path once we are able to trigger it
                // in that case.
                mSuggestionsUiDelegate.getEventReporter().onSurfaceOpened();
                SuggestionsMetrics.recordSurfaceVisible();

                if (isFirstShown) {
                    mAdapter.refreshSuggestions();

                    maybeUpdateContextualSuggestions();

                    // Set the adapter on the RecyclerView after updating it, to avoid sending
                    // notifications that might confuse its internal state.
                    // See https://crbug.com/756514.
                    mRecyclerView.setAdapter(mAdapter);
                    mRecyclerView.scrollToPosition(0);
                    mRecyclerView.getScrollEventReporter().reset();
                }
            }

            @Override
            public void onContentHidden() {
                SuggestionsMetrics.recordSurfaceHidden();
            }

            @Override
            public void onContentStateChanged(@BottomSheet.SheetState int contentState) {
                if (contentState == BottomSheet.SHEET_STATE_HALF) {
                    SuggestionsMetrics.recordSurfaceHalfVisible();
                    mRecyclerView.setScrollEnabled(false);
                } else if (contentState == BottomSheet.SHEET_STATE_FULL) {
                    SuggestionsMetrics.recordSurfaceFullyVisible();
                    mRecyclerView.setScrollEnabled(true);
                }

                updateLogoTransition();
            }

            @Override
            public void onSheetClosed(@StateChangeReason int reason) {
                super.onSheetClosed(reason);

                if (ChromeFeatureList.isEnabled(
                            ChromeFeatureList.CHROME_HOME_DROP_ALL_BUT_FIRST_THUMBNAIL)) {
                    mAdapter.dropAllButFirstNArticleThumbnails(1);
                }
                mRecyclerView.setAdapter(null);
                updateLogoTransition();
            }

            @Override
            public void onSheetOffsetChanged(float heightFraction) {
                mLastSheetHeightFraction = heightFraction;
                updateLogoTransition();
            }
        };

        mLogoDelegate = new LogoDelegateImpl(navigationDelegate, mLogoView, profile);
        updateSearchProviderHasLogo();
        if (mSearchProviderHasLogo) {
            mLogoView.showSearchProviderInitialView();
            loadSearchProviderLogo();
        }
        TemplateUrlService.getInstance().addObserver(this);
        mSheet.getNewTabController().addObserver(this);

        mView.addOnAttachStateChangeListener(new OnAttachStateChangeListener() {
            @Override
            public void onViewAttachedToWindow(View v) {
                mIsAttachedToWindow = true;
                updateLogoVisibility();
                updateLogoTransition();
            }

            @Override
            public void onViewDetachedFromWindow(View v) {
                mIsAttachedToWindow = false;
                updateLogoVisibility();
                updateLogoTransition();
            }
        });

        mView.addOnLayoutChangeListener(
                (View view, int left, int top, int right, int bottom, int oldLeft, int oldTop,
                        int oldRight, int oldBottom) -> { updateLogoTransition(); });

        mRecyclerView.addOnScrollListener(new RecyclerView.OnScrollListener() {
            @Override
            public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                updateLogoTransition();
            }
        });
    }

    @Override
    public View getContentView() {
        return mView;
    }

    @Override
    public List<View> getViewsForPadding() {
        return CollectionUtil.newArrayList(mRecyclerView);
    }

    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public boolean isUsingLightToolbarTheme() {
        return false;
    }

    @Override
    public boolean isIncognitoThemedContent() {
        return false;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mRecyclerView.computeVerticalScrollOffset();
    }

    @Override
    public void destroy() {
        mIsDestroyed = true;
        mSheet.getNewTabController().removeObserver(this);
        mLocationBar.removeUrlFocusChangeListener(this);
        if (mAnimator != null) {
            mAnimator.removeAllUpdateListeners();
            mAnimator.cancel();
        }

        if (mSuggestionsInitialized) {
            mBottomSheetObserver.onDestroy();
            mSuggestionsUiDelegate.onDestroy();
            mTileGroupDelegate.destroy();
            TemplateUrlService.getInstance().removeObserver(this);

            if (mContextualSuggestions != null) mContextualSuggestions.destroy();
        }
    }

    @Override
    public int getType() {
        return BottomSheetContentController.TYPE_SUGGESTIONS;
    }

    @Override
    public boolean applyDefaultTopPadding() {
        return false;
    }

    @Override
    public void scrollToTop() {
        mRecyclerView.smoothScrollToPosition(0);
    }

    @Override
    public void onNewTabShown() {
        mNewTabShown = true;
        updateLogoVisibility();
        maybeUpdateContextualSuggestions();
    }

    @Override
    public void onNewTabHidden() {
        mNewTabShown = false;
        updateLogoVisibility();
    }

    @Override
    public void onTemplateURLServiceChanged() {
        updateSearchProviderHasLogo();
        loadSearchProviderLogo();
        updateLogoVisibility();
        updateLogoTransition();
    }

    @Override
    public void onUrlFocusChange(final boolean hasFocus) {
        if (hasFocus && !isAnimating()) mTransitionFractionBeforeAnimation = mTransitionFraction;

        float startFraction = mTransitionFraction;
        float endFraction = hasFocus ? 1.0f : mTransitionFractionBeforeAnimation;

        if (isAnimating()) mAnimator.cancel();
        mAnimator = ValueAnimator.ofFloat(startFraction, endFraction);
        mAnimator.setDuration(ANIMATION_DURATION_MS);
        mAnimator.setInterpolator(mInterpolator);
        mAnimator.addUpdateListener(animator -> updateLogoTransition());
        mAnimator.start();
    }

    private void maybeUpdateContextualSuggestions() {
        if (mSuggestionsCarousel == null && mContextualSuggestions == null) return;

        Tab activeTab = mSheet.getActiveTab();
        final String currentUrl = activeTab == null ? null : activeTab.getUrl();

        if (mSuggestionsCarousel != null) {
            assert ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_CAROUSEL);
            mSuggestionsCarousel.refresh(mSheet.getContext(), currentUrl);
        }

        if (mContextualSuggestions != null) {
            assert ChromeFeatureList.isEnabled(
                    ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_ABOVE_ARTICLES);
            if (!mSheet.isShowingNewTab()) {
                mContextualSuggestions.setSectionVisiblity(true);
                mContextualSuggestions.refresh(currentUrl);
            } else {
                mContextualSuggestions.setSectionVisiblity(false);
            }
        }
    }

    private void updateSearchProviderHasLogo() {
        mSearchProviderHasLogo = TemplateUrlService.getInstance().doesDefaultSearchEngineHaveLogo();
    }

    /**
     * Loads the search provider logo, if any.
     */
    private void loadSearchProviderLogo() {
        if (!mSearchProviderHasLogo) return;

        mLogoView.showSearchProviderInitialView();

        mLogoDelegate.getSearchProviderLogo(new LogoObserver() {
            @Override
            public void onLogoAvailable(Logo logo, boolean fromCache) {
                if (logo == null && fromCache) return;

                mLogoView.setDelegate(mLogoDelegate);
                mLogoView.updateLogo(logo);
            }
        });
    }

    private void updateLogoTransition() {
        boolean showLogo = shouldShowLogo();

        // If the logo is not shown, reset all transitions.
        if (!showLogo) {
            mControlContainerView.setTranslationY(0);
            mToolbarPullHandle.setTranslationY(0);
            mToolbarShadow.setTranslationY(0);
            mToolbarView.setShownInNtp(false);
            ViewUtils.setAncestorsShouldClipChildren(mControlContainerView, true);
            mRecyclerView.setAlpha(1.0f);
            mRecyclerView.setVisibility(View.VISIBLE);
            return;
        }

        ViewUtils.setAncestorsShouldClipChildren(mControlContainerView, false);

        // Calculate the transition fraction for hiding the logo: 0 means the logo is fully visible,
        // 1 means it is fully invisible. When the URL focus animation is running, that determines
        // the logo movement. Otherwise, the logo moves at the same speed as the scrolling of the
        // RecyclerView.
        boolean hasFocus = mLocationBar.hasFocus();
        if (isAnimating()) {
            mTransitionFraction = (float) mAnimator.getAnimatedValue();
        } else if (hasFocus) {
            mTransitionFraction = 1.0f;
        } else if (mRecyclerView.isFirstItemVisible()) {
            // The computeVerticalScrollOffset method is only accurate if the first item is visible.
            mTransitionFraction =
                    Math.min(1.0f, mRecyclerView.computeVerticalScrollOffset() / mMaxToolbarOffset);
        } else {
            // If the first item is not visible we have scrolled quite far and the transition is
            // complete.
            mTransitionFraction = 1.0f;
        }

        // Transform the sheet height fraction back to pixel scale.
        float rangePx =
                (mSheet.getFullRatio() - mSheet.getPeekRatio()) * mSheet.getSheetContainerHeight();
        float sheetHeightPx = mLastSheetHeightFraction * rangePx;

        // The toolbar follows the logo, except it sticks to the top and bottom of the sheet when it
        // reaches those edges.
        float toolbarOffset =
                Math.min(sheetHeightPx, mMaxToolbarOffset * (1.0f - mTransitionFraction));
        mControlContainerView.setTranslationY(toolbarOffset);
        mToolbarPullHandle.setTranslationY(-toolbarOffset);
        mToolbarShadow.setTranslationY(-toolbarOffset);

        if (toolbarOffset == 0.0f || toolbarOffset == sheetHeightPx) {
            mToolbarView.setShownInNtp(false);
        } else {
            mToolbarView.setShownInNtp(true);
        }

        // Fade out the whole RecyclerView when the URL bar is focused, and fade it in when it loses
        // focus.
        final float alpha;
        if (isAnimating() && hasFocus) {
            alpha = 1.0f - mAnimator.getAnimatedFraction();
        } else if (isAnimating() && !hasFocus) {
            alpha = mAnimator.getAnimatedFraction();
        } else if (hasFocus) {
            alpha = 0.0f;
        } else {
            alpha = 1.0f;
        }
        mRecyclerView.setAlpha(alpha);
        mRecyclerView.setVisibility(alpha == 0.0f ? View.INVISIBLE : View.VISIBLE);
    }

    private void updateLogoVisibility() {
        boolean showLogo = shouldShowLogo();
        mAdapter.setLogoVisibility(showLogo);
        int top = showLogo ? 0 : mToolbarHeight;
        int left = mRecyclerView.getPaddingLeft();
        int right = mRecyclerView.getPaddingRight();
        int bottom = mRecyclerView.getPaddingBottom();
        mRecyclerView.setPadding(left, top, right, bottom);

        mRecyclerView.scrollToPosition(0);
    }

    private boolean shouldShowLogo() {
        return mSearchProviderHasLogo && mNewTabShown && mSheet.isSheetOpen()
                && !mActivity.getTabModelSelector().isIncognitoSelected() && mIsAttachedToWindow;
    }

    private boolean isAnimating() {
        return mAnimator != null && mAnimator.isRunning();
    }
}

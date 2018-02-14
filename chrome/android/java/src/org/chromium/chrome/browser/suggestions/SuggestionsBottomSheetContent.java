// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.animation.ValueAnimator;
import android.content.res.Resources;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
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
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;
import org.chromium.chrome.browser.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
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

    private boolean mSuggestionsInitialized;
    private boolean mIsDestroyed;
    private boolean mSearchProviderHasLogo = true;

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
    private ValueAnimator mAnimator;
    private final Interpolator mInterpolator = new DecelerateInterpolator();

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

        LoadingView loadingView = mView.findViewById(R.id.loading_view);

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
                            updateLogoVisibility();
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
            }

            @Override
            public void onSheetClosed(@StateChangeReason int reason) {
                super.onSheetClosed(reason);

                if (ChromeFeatureList.isEnabled(
                            ChromeFeatureList.CHROME_HOME_DROP_ALL_BUT_FIRST_THUMBNAIL)) {
                    mAdapter.dropAllButFirstNArticleThumbnails(1);
                }
                mRecyclerView.setAdapter(null);
            }
        };

        mLogoDelegate = new LogoDelegateImpl(navigationDelegate, mLogoView, profile);
        updateSearchProviderHasLogo();
        if (mSearchProviderHasLogo) {
            mLogoView.showSearchProviderInitialView();
            loadSearchProviderLogo();
        }
        TemplateUrlService.getInstance().addObserver(this);

        mView.addOnAttachStateChangeListener(new OnAttachStateChangeListener() {
            @Override
            public void onViewAttachedToWindow(View v) {
                updateLogoVisibility();
            }

            @Override
            public void onViewDetachedFromWindow(View v) {
                updateLogoVisibility();
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
        updateLogoVisibility();
        maybeUpdateContextualSuggestions();
    }

    @Override
    public void onNewTabHidden() {
        updateLogoVisibility();
    }

    @Override
    public void onTemplateURLServiceChanged() {
        updateSearchProviderHasLogo();
        loadSearchProviderLogo();
        updateLogoVisibility();
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

    private void updateLogoVisibility() {
        int top = mToolbarHeight;
        int left = mRecyclerView.getPaddingLeft();
        int right = mRecyclerView.getPaddingRight();
        int bottom = mRecyclerView.getPaddingBottom();
        mRecyclerView.setPadding(left, top, right, bottom);

        mRecyclerView.scrollToPosition(0);
    }

    private boolean isAnimating() {
        return mAnimator != null && mAnimator.isRunning();
    }
}

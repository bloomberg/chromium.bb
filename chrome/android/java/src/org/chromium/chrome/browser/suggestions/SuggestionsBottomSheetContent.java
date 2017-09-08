// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.annotation.SuppressLint;
import android.content.res.Resources;
import android.graphics.Rect;
import android.support.annotation.Nullable;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.OnScrollListener;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.TouchDelegate;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ApiCompatibilityUtils;
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
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;
import org.chromium.chrome.browser.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.chrome.browser.widget.FadingShadow;
import org.chromium.chrome.browser.widget.FadingShadowView;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContentController;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetNewTabController;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;

/**
 * Provides content to be displayed inside of the Home tab of bottom sheet.
 */
public class SuggestionsBottomSheetContent implements BottomSheet.BottomSheetContent,
                                                      BottomSheetNewTabController.Observer,
                                                      TemplateUrlServiceObserver {
    private final View mView;
    private final SuggestionsRecyclerView mRecyclerView;
    private final ContextMenuManager mContextMenuManager;
    private final SuggestionsUiDelegateImpl mSuggestionsUiDelegate;
    private final TileGroup.Delegate mTileGroupDelegate;
    @Nullable
    private final SuggestionsCarousel mSuggestionsCarousel;
    private final SuggestionsSheetVisibilityChangeObserver mBottomSheetObserver;
    private final BottomSheet mSheet;
    private final LogoView mLogoView;
    private final LogoDelegateImpl mLogoDelegate;
    private final ViewGroup mToolbarView;

    private boolean mNewTabShown;
    private boolean mSearchProviderHasLogo = true;
    private float mLastSheetHeightFraction = 1f;

    // The sheet height fractions at which the logo transitions start and end.
    private static final float LOGO_TRANSITION_BOTTOM_START = 0.3f;
    private static final float LOGO_TRANSITION_BOTTOM_END = 0.1f;
    private static final float LOGO_TRANSITION_TOP_START = 0.75f;
    private static final float LOGO_TRANSITION_TOP_END = 0.95f;

    public SuggestionsBottomSheetContent(final ChromeActivity activity, final BottomSheet sheet,
            TabModelSelector tabModelSelector, SnackbarManager snackbarManager) {
        SuggestionsDependencyFactory depsFactory = SuggestionsDependencyFactory.getInstance();
        Profile profile = Profile.getLastUsedProfile();
        SuggestionsNavigationDelegate navigationDelegate =
                new SuggestionsNavigationDelegateImpl(activity, profile, sheet, tabModelSelector);
        mSheet = sheet;
        mTileGroupDelegate =
                new TileGroupDelegateImpl(activity, profile, navigationDelegate, snackbarManager);
        mSuggestionsUiDelegate = new SuggestionsUiDelegateImpl(
                depsFactory.createSuggestionSource(profile), depsFactory.createEventReporter(),
                navigationDelegate, profile, sheet, activity.getReferencePool());

        mView = LayoutInflater.from(activity).inflate(
                R.layout.suggestions_bottom_sheet_content, null);
        Resources resources = mView.getResources();
        int backgroundColor = SuggestionsConfig.getBackgroundColor(resources);
        mView.setBackgroundColor(backgroundColor);
        mRecyclerView = mView.findViewById(R.id.recycler_view);
        mRecyclerView.setBackgroundColor(backgroundColor);

        TouchEnabledDelegate touchEnabledDelegate = new TouchEnabledDelegate() {
            @Override
            public void setTouchEnabled(boolean enabled) {
                activity.getBottomSheet().setTouchEnabled(enabled);
            }
        };

        mContextMenuManager =
                new ContextMenuManager(activity, navigationDelegate, touchEnabledDelegate);
        activity.getWindowAndroid().addContextMenuCloseListener(mContextMenuManager);
        mSuggestionsUiDelegate.addDestructionObserver(new DestructionObserver() {
            @Override
            public void onDestroy() {
                activity.getWindowAndroid().removeContextMenuCloseListener(mContextMenuManager);
            }
        });

        UiConfig uiConfig = new UiConfig(mRecyclerView);
        mRecyclerView.init(uiConfig, mContextMenuManager);

        mSuggestionsCarousel =
                ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_CAROUSEL)
                ? new SuggestionsCarousel(uiConfig, mSuggestionsUiDelegate)
                : null;

        final NewTabPageAdapter adapter = new NewTabPageAdapter(mSuggestionsUiDelegate,
                /* aboveTheFoldView = */ null, uiConfig, OfflinePageBridge.getForProfile(profile),
                mContextMenuManager, mTileGroupDelegate, mSuggestionsCarousel);

        mBottomSheetObserver = new SuggestionsSheetVisibilityChangeObserver(this, activity) {
            @Override
            public void onContentShown(boolean isFirstShown) {
                if (isFirstShown) {
                    adapter.refreshSuggestions();
                    mSuggestionsUiDelegate.getEventReporter().onSurfaceOpened();

                    maybeUpdateContextualSuggestions();

                    // Set the adapter on the RecyclerView after updating it, to avoid sending
                    // notifications that might confuse its internal state.
                    // See https://crbug.com/756514.
                    mRecyclerView.setAdapter(adapter);
                    mRecyclerView.scrollToPosition(0);
                    mRecyclerView.getScrollEventReporter().reset();
                }

                SuggestionsMetrics.recordSurfaceVisible();
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
            public void onSheetClosed() {
                super.onSheetClosed();
                mRecyclerView.setAdapter(null);
                updateLogoTransition();
            }

            @Override
            public void onSheetOffsetChanged(float heightFraction) {
                mLastSheetHeightFraction = heightFraction;
                updateLogoTransition();
            }
        };

        initializeShadow();

        final LocationBar locationBar = (LocationBar) sheet.findViewById(R.id.location_bar);
        mRecyclerView.setOnTouchListener(new View.OnTouchListener() {
            @Override
            @SuppressLint("ClickableViewAccessibility")
            public boolean onTouch(View view, MotionEvent motionEvent) {
                if (locationBar != null && locationBar.isUrlBarFocused()) {
                    locationBar.setUrlBarFocus(false);
                }

                // Never intercept the touch event.
                return false;
            }
        });

        mLogoView = mView.findViewById(R.id.search_provider_logo);
        mToolbarView = (ViewGroup) activity.findViewById(R.id.toolbar);
        mLogoDelegate = new LogoDelegateImpl(navigationDelegate, mLogoView, profile);
        updateSearchProviderHasLogo();
        if (mSearchProviderHasLogo) {
            mLogoView.showSearchProviderInitialView();
            loadSearchProviderLogo();
        }
        TemplateUrlService.getInstance().addObserver(this);
        sheet.getNewTabController().addObserver(this);
    }

    @Override
    public View getContentView() {
        return mView;
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
        mBottomSheetObserver.onDestroy();
        mSuggestionsUiDelegate.onDestroy();
        mTileGroupDelegate.destroy();
        TemplateUrlService.getInstance().removeObserver(this);
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
    public void onNewTabShown() {
        mNewTabShown = true;
    }

    @Override
    public void onNewTabHidden() {
        mNewTabShown = false;
    }

    @Override
    public void onTemplateURLServiceChanged() {
        updateSearchProviderHasLogo();
        loadSearchProviderLogo();
        updateLogoTransition();
    }

    private void initializeShadow() {
        final FadingShadowView shadowView = (FadingShadowView) mView.findViewById(R.id.shadow);

        if (FeatureUtilities.isChromeHomeModernEnabled()) {
            ((ViewGroup) mView).removeView(shadowView);
            return;
        }

        shadowView.init(
                ApiCompatibilityUtils.getColor(mView.getResources(), R.color.toolbar_shadow_color),
                FadingShadow.POSITION_TOP);

        mRecyclerView.addOnScrollListener(new OnScrollListener() {
            @Override
            public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                boolean shadowVisible = mRecyclerView.canScrollVertically(-1);
                shadowView.setVisibility(shadowVisible ? View.VISIBLE : View.GONE);
            }
        });
    }

    private void maybeUpdateContextualSuggestions() {
        if (mSuggestionsCarousel == null) return;
        assert ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_CAROUSEL);

        Tab activeTab = mSheet.getActiveTab();
        final String currentUrl = activeTab == null ? null : activeTab.getUrl();

        mSuggestionsCarousel.refresh(mSheet.getContext(), currentUrl);
    }

    private void updateSearchProviderHasLogo() {
        mSearchProviderHasLogo = TemplateUrlService.getInstance().doesDefaultSearchEngineHaveLogo();
    }

    /**
     * Loads the search provider logo (e.g. Google doodle), if any.
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
        boolean showLogo = mSearchProviderHasLogo && mNewTabShown && mSheet.isSheetOpen()
                && FeatureUtilities.isChromeHomeDoodleEnabled();

        if (!showLogo) {
            mLogoView.setVisibility(View.GONE);
            mToolbarView.setTranslationY(0);
            mRecyclerView.setTranslationY(0);
            mSheet.setTouchDelegate(null);
            ViewUtils.setAncestorsShouldClipChildren(mToolbarView, true);
            return;
        }

        mLogoView.setVisibility(View.VISIBLE);
        ViewUtils.setAncestorsShouldClipChildren(mToolbarView, false);

        // TODO(mvanouwerkerk): Consider using Material animation curves.
        final float transitionFraction;
        if (mLastSheetHeightFraction > LOGO_TRANSITION_TOP_START) {
            float range = LOGO_TRANSITION_TOP_END - LOGO_TRANSITION_TOP_START;
            transitionFraction =
                    Math.min((mLastSheetHeightFraction - LOGO_TRANSITION_TOP_START) / range, 1.0f);
        } else if (mLastSheetHeightFraction < LOGO_TRANSITION_BOTTOM_START) {
            float range = LOGO_TRANSITION_BOTTOM_START - LOGO_TRANSITION_BOTTOM_END;
            transitionFraction = Math.min(
                    (LOGO_TRANSITION_BOTTOM_START - mLastSheetHeightFraction) / range, 1.0f);
        } else {
            transitionFraction = 0.0f;
        }

        ViewGroup.MarginLayoutParams logoParams =
                (ViewGroup.MarginLayoutParams) mLogoView.getLayoutParams();
        mLogoView.setTranslationY(logoParams.topMargin * -transitionFraction);
        mLogoView.setAlpha(Math.max(0.0f, 1.0f - (transitionFraction * 3.0f)));

        float maxToolbarOffset = logoParams.height + logoParams.topMargin + logoParams.bottomMargin;
        float toolbarOffset = maxToolbarOffset * (1.0f - transitionFraction);
        mToolbarView.setTranslationY(toolbarOffset);
        mRecyclerView.setTranslationY(toolbarOffset);

        if (transitionFraction == 0.f) {
            mSheet.setTouchDelegate(new NtpTouchDelegate(mSheet, mToolbarView, mLogoView));
        } else {
            mSheet.setTouchDelegate(null);
        }
    }

    /**
     * Routes touch events when the NTP is showing a search provider logo. When Views are translated
     * outside of their parent's bounds, they typically cannot receive touch input. This class sends
     * touch events to the search provider logo, toolbar, and menu button.
     */
    private static class NtpTouchDelegate extends TouchDelegate {
        private final TouchDelegate mLogoTouchDelegate;
        private final TouchDelegate mMenuButtonTouchDelegate;
        private final TouchDelegate mToolbarTouchDelegate;
        private final Rect mLogoRect;
        private final Rect mMenuButtonRect;
        private final Rect mToolbarRect;

        /**
         * Creates a new NTP touch delegate.
         * @param bottomSheet The {@link BottomSheet} that contains the
         *                    SuggestionBottomSheetContent. Used to determine hit rects for children
         *                    contained within the BottomSheet.
         * @param toolbarView The {@link View} containing the toolbar.
         * @param logoView    The {@link View} containing the search provider logo.
         */
        public NtpTouchDelegate(View bottomSheet, View toolbarView, View logoView) {
            // We don't actually rely on the superclass for anything, so initialize it with dummy
            // values.
            super(new Rect(), bottomSheet);

            int[] parentLocationOnScreen = new int[2];
            bottomSheet.getLocationOnScreen(parentLocationOnScreen);

            mLogoRect = createTouchDelegateRect(logoView, parentLocationOnScreen);

            View menuButton = toolbarView.findViewById(R.id.menu_button);
            mMenuButtonRect = createTouchDelegateRect(menuButton, parentLocationOnScreen);

            mToolbarRect = createTouchDelegateRect(toolbarView, parentLocationOnScreen);

            mLogoTouchDelegate = new TouchDelegate(mLogoRect, logoView);
            mMenuButtonTouchDelegate = new TouchDelegate(mMenuButtonRect, menuButton);
            mToolbarTouchDelegate = new TouchDelegate(mToolbarRect, toolbarView);
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            int x = (int) event.getX();
            int y = (int) event.getY();

            if (mLogoRect.contains(x, y)) return mLogoTouchDelegate.onTouchEvent(event);
            if (mMenuButtonRect.contains(x, y)) return mMenuButtonTouchDelegate.onTouchEvent(event);
            if (mToolbarRect.contains(x, y)) return mToolbarTouchDelegate.onTouchEvent(event);

            return false;
        }

        private Rect createTouchDelegateRect(View childView, int[] parentLocationOnScreen) {
            int[] childLocationOnScreen = new int[2];
            childView.getLocationOnScreen(childLocationOnScreen);

            Rect touchRect = new Rect();
            touchRect.top = childLocationOnScreen[1] - parentLocationOnScreen[1];
            touchRect.bottom = touchRect.top + childView.getMeasuredHeight();
            touchRect.left = childLocationOnScreen[0] - parentLocationOnScreen[0];
            touchRect.right = touchRect.left + childView.getMeasuredWidth();
            return touchRect;
        }
    }
}

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.annotation.SuppressLint;
import android.content.res.Resources;
import android.support.annotation.Nullable;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.OnScrollListener;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
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
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;
import org.chromium.chrome.browser.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.widget.FadingShadow;
import org.chromium.chrome.browser.widget.FadingShadowView;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContentController;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetNewTabController;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;
import org.chromium.ui.widget.Toast;

import java.util.List;
import java.util.Locale;

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
    private final View mToolbarView;

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
        mTileGroupDelegate = new TileGroupDelegateImpl(
                activity, profile, tabModelSelector, navigationDelegate, snackbarManager);
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
                    mRecyclerView.setAdapter(adapter);

                    mRecyclerView.scrollToPosition(0);
                    adapter.refreshSuggestions();
                    mSuggestionsUiDelegate.getEventReporter().onSurfaceOpened();
                    mRecyclerView.getScrollEventReporter().reset();

                    maybeUpdateContextualSuggestions();
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
                updateLogoTransition();
            }

            @Override
            public void onSheetClosed() {
                super.onSheetClosed();
                mRecyclerView.setAdapter(null);
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
        mToolbarView = activity.findViewById(R.id.control_container);
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

        if (SuggestionsConfig.useModern()) {
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

        if (mSheet.getActiveTab() == null) {
            mSuggestionsCarousel.clearSuggestions();
            return;
        }

        final String url = mSheet.getActiveTab().getUrl();

        // Do nothing if there are already suggestions in the carousel for the current context.
        if (TextUtils.equals(url, mSuggestionsCarousel.getCurrentCarouselContextUrl())) return;

        String text = String.format(Locale.US, "Fetching contextual suggestions...");
        Toast.makeText(mRecyclerView.getContext(), text, Toast.LENGTH_SHORT).show();
        mSuggestionsUiDelegate.getSuggestionsSource().fetchContextualSuggestions(
                url, new Callback<List<SnippetArticle>>() {
                    @Override
                    public void onResult(List<SnippetArticle> contextualSuggestions) {
                        mSuggestionsCarousel.newContextualSuggestionsAvailable(
                                url, contextualSuggestions);
                    }
                });
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
        boolean showLogo = mSearchProviderHasLogo && mNewTabShown && SuggestionsConfig.useModern()
                && mBottomSheetObserver.isVisible();

        if (!showLogo) {
            mLogoView.setVisibility(View.GONE);
            mToolbarView.setTranslationY(0);
            mRecyclerView.setTranslationY(0);
            return;
        }

        mLogoView.setVisibility(View.VISIBLE);

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
    }
}

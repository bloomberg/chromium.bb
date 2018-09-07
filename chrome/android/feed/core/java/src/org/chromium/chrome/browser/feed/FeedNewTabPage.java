// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ScrollView;

import com.google.android.libraries.feed.api.scope.FeedProcessScope;
import com.google.android.libraries.feed.api.scope.FeedStreamScope;
import com.google.android.libraries.feed.api.stream.NonDismissibleHeader;
import com.google.android.libraries.feed.api.stream.Stream;
import com.google.android.libraries.feed.host.action.ActionApi;
import com.google.android.libraries.feed.host.stream.CardConfiguration;
import com.google.android.libraries.feed.host.stream.SnackbarApi;
import com.google.android.libraries.feed.host.stream.StreamConfiguration;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.feed.action.FeedActionHandler;
import org.chromium.chrome.browser.native_page.NativePageHost;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPageLayout;
import org.chromium.chrome.browser.ntp.SnapScrollHelper;
import org.chromium.chrome.browser.ntp.snippets.SectionHeaderView;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.chrome.browser.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.chrome.browser.widget.displaystyle.MarginResizer;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;
import org.chromium.ui.UiUtils;

import java.util.Arrays;

/**
 * Provides a new tab page that displays an interest feed rendered list of content suggestions.
 */
public class FeedNewTabPage extends NewTabPage {
    private final FeedNewTabPageMediator mMediator;

    private UiConfig mUiConfig;
    private FrameLayout mRootView;

    // Used when Feed is enabled.
    private @Nullable Stream mStream;
    private @Nullable FeedImageLoader mImageLoader;
    private @Nullable StreamLifecycleManager mStreamLifecycleManager;
    private @Nullable SectionHeaderView mSectionHeaderView;

    // Used when Feed is disabled by policy.
    private @Nullable ScrollView mScrollViewForPolicy;

    private class BasicSnackbarApi implements SnackbarApi {
        @Override
        public void show(String message) {
            mNewTabPageManager.getSnackbarManager().showSnackbar(
                    Snackbar.make(message, new SnackbarManager.SnackbarController() {},
                            Snackbar.TYPE_ACTION, Snackbar.UMA_FEED_NTP_STREAM));
        }
    }

    private static class BasicStreamConfiguration implements StreamConfiguration {
        public BasicStreamConfiguration() {}

        @Override
        public int getPaddingStart() {
            return 0;
        }
        @Override
        public int getPaddingEnd() {
            return 0;
        }
        @Override
        public int getPaddingTop() {
            return 0;
        }
        @Override
        public int getPaddingBottom() {
            return 0;
        }
    }

    private static class BasicCardConfiguration implements CardConfiguration {
        private final Resources mResources;
        private final UiConfig mUiConfig;
        private final int mCornerRadius;
        private final Drawable mCardBackground;
        private final int mCardMargin;
        private final int mCardWideMargin;

        public BasicCardConfiguration(Resources resources, UiConfig uiConfig) {
            mResources = resources;
            mUiConfig = uiConfig;
            mCornerRadius = mResources.getDimensionPixelSize(
                    R.dimen.content_suggestions_card_modern_corner_radius);
            mCardBackground = ApiCompatibilityUtils.getDrawable(
                    mResources, R.drawable.content_card_modern_background);
            mCardMargin = mResources.getDimensionPixelSize(
                    R.dimen.content_suggestions_card_modern_margin);
            mCardWideMargin =
                    mResources.getDimensionPixelSize(R.dimen.ntp_wide_card_lateral_margins);
        }

        @Override
        public int getDefaultCornerRadius() {
            return mCornerRadius;
        }

        @Override
        public Drawable getCardBackground() {
            return mCardBackground;
        }

        @Override
        public int getCardBottomMargin() {
            return mCardMargin;
        }

        @Override
        public int getCardStartMargin() {
            return mUiConfig.getCurrentDisplayStyle().horizontal == HorizontalDisplayStyle.WIDE
                    ? mCardWideMargin
                    : mCardMargin;
        }

        @Override
        public int getCardEndMargin() {
            return mUiConfig.getCurrentDisplayStyle().horizontal == HorizontalDisplayStyle.WIDE
                    ? mCardWideMargin
                    : mCardMargin;
        }
    }

    /**
     * Provides the additional capabilities needed for the {@link FeedNewTabPage} container view.
     */
    private class RootView extends FrameLayout {
        public RootView(Context context) {
            super(context);
        }

        @Override
        protected void onConfigurationChanged(Configuration newConfig) {
            super.onConfigurationChanged(newConfig);
            mUiConfig.updateDisplayStyle();
        }

        @Override
        public boolean onInterceptTouchEvent(MotionEvent ev) {
            return !mMediator.getTouchEnabled() || mFakeboxDelegate.isUrlBarFocused()
                    || super.onInterceptTouchEvent(ev);
        }
    }

    /**
     * Constructs a new FeedNewTabPage.
     *
     * @param activity The containing {@link ChromeActivity}.
     * @param nativePageHost The host for this native page.
     * @param tabModelSelector The {@link TabModelSelector} for the containing activity.
     */
    public FeedNewTabPage(ChromeActivity activity, NativePageHost nativePageHost,
            TabModelSelector tabModelSelector) {
        super(activity, nativePageHost, tabModelSelector);

        LayoutInflater inflater = LayoutInflater.from(activity);
        mNewTabPageLayout = (NewTabPageLayout) inflater.inflate(R.layout.new_tab_page_layout, null);

        // Mediator should be created before any Stream changes.
        mMediator = new FeedNewTabPageMediator(
                this, new SnapScrollHelper(mNewTabPageManager, mNewTabPageLayout));

        // Don't store a direct reference to the activity, because it might change later if the tab
        // is reparented.
        // TODO(twellington): Move this somewhere it can be shared with NewTabPageView?
        Runnable closeContextMenuCallback = () -> mTab.getActivity().closeContextMenu();
        ContextMenuManager contextMenuManager =
                new ContextMenuManager(mNewTabPageManager.getNavigationDelegate(), mMediator,
                        closeContextMenuCallback, false);
        mTab.getWindowAndroid().addContextMenuCloseListener(contextMenuManager);

        mNewTabPageLayout.initialize(mNewTabPageManager, mTab, mTileGroupDelegate,
                mSearchProviderHasLogo,
                TemplateUrlService.getInstance().isDefaultSearchEngineGoogle(), mMediator,
                contextMenuManager, mUiConfig);
    }

    @Override
    protected void initializeMainView(Context context) {
        int topPadding = context.getResources().getDimensionPixelOffset(R.dimen.tab_strip_height);
        mRootView = new RootView(context);
        mRootView.setLayoutParams(new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
        mRootView.setPadding(0, topPadding, 0, 0);
        mUiConfig = new UiConfig(mRootView);
    }

    @Override
    public void destroy() {
        super.destroy();
        mMediator.destroy();
        if (mImageLoader != null) mImageLoader.destroy();
        if (mStreamLifecycleManager != null) mStreamLifecycleManager.destroy();
    }

    @Override
    public View getView() {
        return mRootView;
    }

    @Override
    protected void saveLastScrollPosition() {
        // This behavior is handled by the StreamLifecycleManager and the Feed library.
    }

    @Override
    protected void scrollToSuggestions() {
        // TODO(twellington): implement this method.
    }

    @Override
    public boolean shouldCaptureThumbnail() {
        return mNewTabPageLayout.shouldCaptureThumbnail() || mMediator.shouldCaptureThumbnail();
    }

    @Override
    public void captureThumbnail(Canvas canvas) {
        mNewTabPageLayout.onPreCaptureThumbnail();
        ViewUtils.captureBitmap(mRootView, canvas);
        mMediator.onThumbnailCaptured();
    }

    /** @return The {@link Stream} that this class holds. */
    Stream getStream() {
        return mStream;
    }

    /**
     * Create a {@link Stream} for this class.
     */
    void createStream() {
        if (mScrollViewForPolicy != null) {
            mRootView.removeView(mScrollViewForPolicy);
            mScrollViewForPolicy = null;
        }

        FeedProcessScope feedProcessScope = FeedProcessScopeFactory.getFeedProcessScope();
        Activity activity = mTab.getActivity();
        Profile profile = mTab.getProfile();

        mImageLoader = new FeedImageLoader(profile, activity);
        ActionApi actionApi = new FeedActionHandler(mNewTabPageManager.getNavigationDelegate(),
                () -> FeedProcessScopeFactory.getFeedScheduler().onSuggestionConsumed());
        FeedOfflineIndicator offlineIndicator = FeedProcessScopeFactory.getFeedOfflineIndicator();
        FeedStreamScope streamScope =
                feedProcessScope
                        .createFeedStreamScopeBuilder(activity, mImageLoader, actionApi,
                                new BasicStreamConfiguration(),
                                new BasicCardConfiguration(activity.getResources(), mUiConfig),
                                new BasicSnackbarApi(), new FeedBasicLogging(), offlineIndicator)
                        .build();

        mStream = streamScope.getStream();
        mStreamLifecycleManager = new StreamLifecycleManager(mStream, activity, mTab);

        LayoutInflater inflater = LayoutInflater.from(activity);
        mSectionHeaderView = (SectionHeaderView) inflater.inflate(
                R.layout.new_tab_page_snippets_expandable_header, null);

        // TODO(huayinz): Add MarginResizer for sign-in promo under issue 860051 or 860043,
        // depending on which one lands first.
        Resources resources = mSectionHeaderView.getResources();
        int defaultMargin =
                resources.getDimensionPixelSize(R.dimen.content_suggestions_card_modern_margin);
        int wideMargin = resources.getDimensionPixelSize(R.dimen.ntp_wide_card_lateral_margins);
        MarginResizer.createAndAttach(mSectionHeaderView, mUiConfig, defaultMargin, wideMargin);

        View view = mStream.getView();
        view.setBackgroundColor(Color.WHITE);
        mRootView.addView(view);

        UiUtils.removeViewFromParent(mNewTabPageLayout);
        UiUtils.removeViewFromParent(mSectionHeaderView);
        mStream.setHeaderViews(Arrays.asList(new NonDismissibleHeader(mNewTabPageLayout),
                new NonDismissibleHeader(mSectionHeaderView)));
        // Explicitly request focus on the scroll container to avoid UrlBar being focused after
        // the scroll container for policy is removed.
        view.requestFocus();
    }

    /**
     * @return The {@link ScrollView} for displaying content for supervised user or enterprise
     *         policy.
     */
    ScrollView getScrollViewForPolicy() {
        return mScrollViewForPolicy;
    }

    /**
     * Create a {@link ScrollView} for displaying content for supervised user or enterprise policy.
     */
    void createScrollViewForPolicy() {
        if (mStream != null) {
            mRootView.removeView(mStream.getView());
            mStreamLifecycleManager.destroy();
            mStream = null;
            mSectionHeaderView = null;
        }

        mScrollViewForPolicy = new ScrollView(mTab.getActivity());
        mScrollViewForPolicy.setBackgroundColor(Color.WHITE);

        // Make scroll view focusable so that it is the next focusable view when the url bar clears
        // focus.
        mScrollViewForPolicy.setFocusable(true);
        mScrollViewForPolicy.setFocusableInTouchMode(true);
        mScrollViewForPolicy.setContentDescription(
                mScrollViewForPolicy.getResources().getString(R.string.accessibility_new_tab_page));

        UiUtils.removeViewFromParent(mNewTabPageLayout);
        mScrollViewForPolicy.addView(mNewTabPageLayout);
        mRootView.addView(mScrollViewForPolicy);
        mScrollViewForPolicy.requestFocus();
    }

    /** @return The {@link SectionHeaderView} for the Feed section header. */
    SectionHeaderView getSectionHeaderView() {
        return mSectionHeaderView;
    }

    /**
     * Configures the {@link FeedNewTabPage} for testing.
     * @param inTestMode Whether test mode is enabled. If true, test implementations of Feed
     *                   interfaces will be used to create the {@link FeedProcessScope}. If false,
     *                   the FeedProcessScope will be reset.
     */
    @VisibleForTesting
    public static void setInTestMode(boolean inTestMode) {
        if (inTestMode) {
            FeedProcessScopeFactory.createFeedProcessScopeForTesting(new TestFeedScheduler(),
                    new TestNetworkClient(), new TestFeedOfflineIndicator());
        } else {
            FeedProcessScopeFactory.clearFeedProcessScopeForTesting();
        }
    }
}

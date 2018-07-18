// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;

import com.google.android.libraries.feed.api.scope.FeedProcessScope;
import com.google.android.libraries.feed.api.scope.FeedStreamScope;
import com.google.android.libraries.feed.api.stream.ScrollListener;
import com.google.android.libraries.feed.api.stream.Stream;
import com.google.android.libraries.feed.host.stream.CardConfiguration;
import com.google.android.libraries.feed.host.stream.SnackbarApi;
import com.google.android.libraries.feed.host.stream.StreamConfiguration;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.NativePageHost;
import org.chromium.chrome.browser.feed.action.FeedActionHandler;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.ContextMenuManager.TouchEnabledDelegate;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPageLayout;
import org.chromium.chrome.browser.ntp.SnapScrollHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegateImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;

import java.util.Arrays;

/**
 * Provides a new tab page that displays an interest feed rendered list of content suggestions.
 */
public class FeedNewTabPage
        extends NewTabPage implements TouchEnabledDelegate, NewTabPageLayout.ScrollDelegate {
    private final StreamLifecycleManager mStreamLifecycleManager;
    private final Stream mStream;
    private final ScrollListener mStreamScrollListener;
    private final SnapScrollHelper mSnapScrollHelper;

    private FrameLayout mRootView;
    private FeedImageLoader mImageLoader;

    private static class DummySnackbarApi implements SnackbarApi {
        // TODO: implement snackbar functionality.
        @Override
        public void show(String message) {}
    }

    private static class BasicStreamConfiguration implements StreamConfiguration {
        private final Resources mResources;
        private final int mPadding;

        public BasicStreamConfiguration(Resources resources) {
            mResources = resources;
            mPadding = mResources.getDimensionPixelSize(
                    R.dimen.content_suggestions_card_modern_margin);
        }

        @Override
        public int getPaddingStart() {
            return mPadding;
        }
        @Override
        public int getPaddingEnd() {
            return mPadding;
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
        private final int mCornerRadius;
        private final Drawable mCardBackground;
        private final int mCardMarginBottom;

        public BasicCardConfiguration(Resources resources) {
            mResources = resources;
            mCornerRadius = mResources.getDimensionPixelSize(
                    R.dimen.content_suggestions_card_modern_corner_radius);
            mCardBackground = ApiCompatibilityUtils.getDrawable(
                    mResources, R.drawable.content_card_modern_background);
            mCardMarginBottom = mResources.getDimensionPixelSize(
                    R.dimen.content_suggestions_card_modern_margin);
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
            return mCardMarginBottom;
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

        FeedProcessScope feedProcessScope = FeedProcessScopeFactory.getFeedProcessScope();
        Tab tab = nativePageHost.getActiveTab();
        Profile profile = tab.getProfile();
        mImageLoader = new FeedImageLoader(profile, activity);
        SuggestionsNavigationDelegateImpl navigationDelegate =
                new SuggestionsNavigationDelegateImpl(
                        activity, profile, nativePageHost, tabModelSelector);
        FeedStreamScope streamScope =
                feedProcessScope
                        .createFeedStreamScopeBuilder(activity, mImageLoader,
                                new FeedActionHandler(navigationDelegate),
                                new BasicStreamConfiguration(activity.getResources()),
                                new BasicCardConfiguration(activity.getResources()),
                                new DummySnackbarApi())
                        .build();

        mStream = streamScope.getStream();
        mStreamLifecycleManager = new StreamLifecycleManager(mStream, activity, tab);
        mSnapScrollHelper =
                new SnapScrollHelper(mNewTabPageManager, mNewTabPageLayout, mStream.getView());

        mStream.getView().setBackgroundColor(Color.WHITE);
        mRootView.addView(mStream.getView());

        mStream.setHeaderViews(Arrays.asList(mNewTabPageLayout));

        // Listen for layout changes on the NewTabPageView itself to catch changes in scroll
        // position that are due to layout changes after e.g. device rotation. This contrasts with
        // regular scrolling, which is observed through an OnScrollListener.
        mRootView.addOnLayoutChangeListener(
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    mSnapScrollHelper.handleScroll();
                });

        mStreamScrollListener = new ScrollListener() {
            @Override
            public void onScrollStateChanged(int state) {}

            @Override
            public void onScrolled(int dx, int dy) {
                mSnapScrollHelper.handleScroll();
            }
        };
        mStream.addScrollListener(mStreamScrollListener);
    }

    @Override
    protected void initializeMainView(Context context) {
        int topPadding = context.getResources().getDimensionPixelOffset(R.dimen.tab_strip_height);
        mRootView = new FrameLayout(context);
        mRootView.setLayoutParams(new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
        mRootView.setPadding(0, topPadding, 0, 0);

        // Don't store a direct reference to the activity, because it might change later if the tab
        // is reparented.
        // TODO(twellington): Move this somewhere it can be shared with NewTabPageView?
        Runnable closeContextMenuCallback = () -> mTab.getActivity().closeContextMenu();
        ContextMenuManager contextMenuManager =
                new ContextMenuManager(mNewTabPageManager.getNavigationDelegate(),
                        this::setTouchEnabled, closeContextMenuCallback);
        mTab.getWindowAndroid().addContextMenuCloseListener(contextMenuManager);

        LayoutInflater inflater = LayoutInflater.from(context);
        mNewTabPageLayout = (NewTabPageLayout) inflater.inflate(R.layout.new_tab_page_layout, null);
        mNewTabPageLayout.initialize(mNewTabPageManager, mTab, mTileGroupDelegate,
                mSearchProviderHasLogo,
                TemplateUrlService.getInstance().isDefaultSearchEngineGoogle(), this,
                contextMenuManager, new UiConfig(mRootView));
    }

    @Override
    public void destroy() {
        super.destroy();
        mStream.removeScrollListener(mStreamScrollListener);
        mImageLoader.destroy();
        mStreamLifecycleManager.destroy();
    }

    @Override
    public View getView() {
        return mRootView;
    }

    @Override
    protected void restoreLastScrollPosition() {
        // This behavior is handled by the Feed library.
    }

    @Override
    protected void scrollToSuggestions() {
        // TODO(twellington): implement this method.
    }

    @Override
    public boolean shouldCaptureThumbnail() {
        // TODO(twellington): add more logic to this method that also takes into account other
        // UI changes that should trigger a thumbnail capture.
        return mNewTabPageLayout.shouldCaptureThumbnail();
    }

    @Override
    public void captureThumbnail(Canvas canvas) {
        mNewTabPageLayout.onPreCaptureThumbnail();
        ViewUtils.captureBitmap(mRootView, canvas);
    }

    // TouchEnabledDelegate interface.

    @Override
    public void setTouchEnabled(boolean enabled) {
        // TODO(twellington): implement this method.
    }

    // ScrollDelegate interface.

    @Override
    public boolean isScrollViewInitialized() {
        // During startup the view may not be fully initialized, so we check to see if some basic
        // view properties (height of the RecyclerView) are sane.
        return mStream != null && mStream.getView().getHeight() > 0;
    }

    @Override
    public int getVerticalScrollOffset() {
        // This method returns a valid vertical scroll offset only when the first header view in the
        // Stream is visible. In all other cases, this returns 0.
        if (!isScrollViewInitialized()) return 0;

        int firstChildTop = mStream.getChildTopAt(0);
        return firstChildTop != Stream.POSITION_NOT_KNOWN ? -firstChildTop : 0;
    }

    @Override
    public boolean isChildVisibleAtPosition(int position) {
        return isScrollViewInitialized() && mStream.isChildAtPositionVisible(position);
    }

    @Override
    public void snapScroll() {
        if (!isScrollViewInitialized()) return;

        int initialScroll = getVerticalScrollOffset();
        int scrollTo = mSnapScrollHelper.calculateSnapPosition(initialScroll);

        // Calculating the snap position should be idempotent.
        assert scrollTo == mSnapScrollHelper.calculateSnapPosition(scrollTo);
        mStream.smoothScrollBy(0, scrollTo - initialScroll);
    }
}

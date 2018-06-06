// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.FrameLayout;

import com.google.android.libraries.feed.api.common.ThreadUtils;
import com.google.android.libraries.feed.api.scope.FeedProcessScope;
import com.google.android.libraries.feed.api.scope.FeedStreamScope;
import com.google.android.libraries.feed.api.stream.Stream;
import com.google.android.libraries.feed.common.time.SystemClockImpl;
import com.google.android.libraries.feed.host.config.Configuration;
import com.google.android.libraries.feed.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.host.config.DebugBehavior;
import com.google.android.libraries.feed.host.stream.CardConfiguration;
import com.google.android.libraries.feed.host.stream.StreamConfiguration;
import com.google.android.libraries.feed.hostimpl.logging.LoggingApiImpl;
import com.google.android.libraries.feed.hostimpl.scheduler.TimeoutScheduler;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.BasicNativePage;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.NativePageHost;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.feed.action.FeedActionHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegateImpl;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.concurrent.Executors;

/**
 * Provides a new tab page that displays an interest feed rendered list of content suggestions.
 */
public class FeedNewTabPage extends BasicNativePage {
    private static FeedProcessScope sFeedProcessScope;

    private FrameLayout mRootView;
    private String mTitle;
    private FeedImageLoader mImageLoader;

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
            return mPadding;
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
        public float getCardBottomPadding() {
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
        super(activity, nativePageHost);

        Profile profile = nativePageHost.getActiveTab().getProfile();
        Configuration configHostApi =
                new Configuration.Builder()
                        .put(ConfigKey.FEED_SERVER_HOST, "https://www.google.com")
                        .put(ConfigKey.FEED_SERVER_PATH_AND_PARAMS,
                                "/httpservice/noretry/NowStreamService/FeedQuery")
                        .put(ConfigKey.SESSION_LIFETIME_MS, 300000L)
                        .build();

        if (sFeedProcessScope == null) {
            sFeedProcessScope =
                    new FeedProcessScope
                            .Builder(configHostApi, Executors.newSingleThreadExecutor(),
                                    new LoggingApiImpl(), new FeedNetworkBridge(profile),
                                    new TimeoutScheduler(new ThreadUtils(), new SystemClockImpl(),
                                            configHostApi),
                                    DebugBehavior.SILENT)
                            .build();
        }

        mImageLoader = new FeedImageLoader(profile, activity);
        SuggestionsNavigationDelegateImpl navigationDelegate =
                new SuggestionsNavigationDelegateImpl(
                        activity, profile, nativePageHost, tabModelSelector);
        FeedStreamScope streamScope =
                sFeedProcessScope
                        .createFeedStreamScopeBuilder(activity, mImageLoader,
                                new FeedActionHandler(navigationDelegate),
                                new BasicStreamConfiguration(activity.getResources()),
                                new BasicCardConfiguration(activity.getResources()), configHostApi)
                        .build();

        Stream stream = streamScope.getStream();
        stream.onCreate(null);
        stream.getView().setBackgroundColor(Color.WHITE);
        mRootView.addView(stream.getView());

        // TODO(skym): This is a work around for outstanding Feed bug.
        stream.triggerRefresh();

        // TODO(https://crbug.com/803317): Call appropriate lifecycle methods.
    }

    @Override
    protected void initialize(ChromeActivity activity, NativePageHost host) {
        mRootView = new FrameLayout(activity);
        mRootView.setLayoutParams(new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
        mTitle = activity.getResources().getString(org.chromium.chrome.R.string.button_new_tab);
    }

    @Override
    public void destroy() {
        super.destroy();
        mImageLoader.destroy();
    }

    @Override
    public View getView() {
        return mRootView;
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public String getHost() {
        return UrlConstants.NTP_HOST;
    }
}

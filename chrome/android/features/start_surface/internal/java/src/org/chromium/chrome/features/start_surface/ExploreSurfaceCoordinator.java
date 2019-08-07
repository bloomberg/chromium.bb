// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.app.Activity;
import android.support.annotation.Nullable;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import com.google.android.libraries.feed.api.client.stream.Stream;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.feed.FeedProcessScopeFactory;
import org.chromium.chrome.browser.feed.FeedSurfaceCoordinator;
import org.chromium.chrome.browser.feed.StreamLifecycleManager;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The coordinator to control the explore surface. */
class ExploreSurfaceCoordinator implements FeedSurfaceCoordinator.FeedSurfaceDelegate {
    private final ChromeActivity mActivity;
    @Nullable
    private final View mHeaderView;
    private final PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private final FeedSurfaceCreator mFeedSurfaceCreator;

    // mExploreSurfaceNavigationDelegate is lightweight, we keep it across FeedSurfaceCoordinators
    // after creating it during the first show.
    private ExploreSurfaceNavigationDelegate mExploreSurfaceNavigationDelegate;

    /** Interface to create {@link FeedSurfaceCoordinator} */
    interface FeedSurfaceCreator {
        /**
         * Creates the {@link FeedSurfaceCoordinator} for the specified mode.
         * @param isIncognito Whether it is in incognito mode.
         * @return The {@link FeedSurfaceCoordinator}.
         */
        FeedSurfaceCoordinator createFeedSurfaceCoordinator(boolean isIncognito);
    }

    ExploreSurfaceCoordinator(ChromeActivity activity, ViewGroup parentView,
            @Nullable View headerView, PropertyModel containerPropertyModel) {
        mActivity = activity;
        mHeaderView = headerView;

        mPropertyModelChangeProcessor = PropertyModelChangeProcessor.create(
                containerPropertyModel, parentView, ExploreSurfaceViewBinder::bind);
        mFeedSurfaceCreator = new FeedSurfaceCreator() {
            @Override
            public FeedSurfaceCoordinator createFeedSurfaceCoordinator(boolean isIncognito) {
                return internalCreateFeedSurfaceCoordinator(isIncognito);
            }
        };
    }

    /**
     * Gets the {@link FeedSurfaceCreator}.
     * @return the {@link FeedSurfaceCreator}.
     */
    FeedSurfaceCreator getFeedSurfaceCreator() {
        return mFeedSurfaceCreator;
    }

    // Implements FeedSurfaceCoordinator.FeedSurfaceDelegate.
    @Override
    public StreamLifecycleManager createStreamLifecycleManager(Stream stream, Activity activity) {
        return new ExploreSurfaceStreamLifecycleManager(stream, activity);
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        return false;
    }

    private FeedSurfaceCoordinator internalCreateFeedSurfaceCoordinator(boolean isIncognito) {
        if (mExploreSurfaceNavigationDelegate == null)
            mExploreSurfaceNavigationDelegate = new ExploreSurfaceNavigationDelegate(mActivity);
        mExploreSurfaceNavigationDelegate.setIncognito(isIncognito);

        ExploreSurfaceActionHandler exploreSurfaceActionHandler =
                new ExploreSurfaceActionHandler(mExploreSurfaceNavigationDelegate,
                        FeedProcessScopeFactory.getFeedConsumptionObserver(),
                        FeedProcessScopeFactory.getFeedOfflineIndicator(),
                        OfflinePageBridge.getForProfile(isIncognito
                                        ? Profile.getLastUsedProfile().getOffTheRecordProfile()
                                        : Profile.getLastUsedProfile()),
                        FeedProcessScopeFactory.getFeedLoggingBridge());
        return new FeedSurfaceCoordinator(
                mActivity, null, null, mHeaderView, exploreSurfaceActionHandler, isIncognito, this);
        // TODO(crbug.com/982018): Customize surface background for incognito and dark mode.
        // TODO(crbug.com/982018): Hide signin promo UI in incognito mode.
    }
}
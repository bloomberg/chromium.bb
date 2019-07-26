// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.app.Activity;
import android.view.MotionEvent;
import android.view.ViewGroup;

import com.google.android.libraries.feed.api.client.stream.Stream;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.feed.FeedProcessScopeFactory;
import org.chromium.chrome.browser.feed.FeedSurfaceCoordinator;
import org.chromium.chrome.browser.feed.StreamLifecycleManager;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.start_surface.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The coordinator to control the explore surface. */
class ExploreSurfaceCoordinator implements FeedSurfaceCoordinator.FeedSurfaceDelegate,
                                           ExploreSurfaceViewBinder.FeedSurfaceDelegate {
    private final ChromeActivity mActivity;
    private final PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    // mExploreSurfaceNavigationDelegate is lightweight, we keep it across FeedSurfaceCoordinators
    // after creating it during the first show.
    private ExploreSurfaceNavigationDelegate mExploreSurfaceNavigationDelegate;

    ExploreSurfaceCoordinator(ChromeActivity activity, ViewGroup parentView,
            int bottomControlsHeight, PropertyModel containerPropertyModel) {
        mActivity = activity;

        int toolbarHeight =
                mActivity.getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);
        int topMargin = ReturnToChromeExperimentsUtil.shouldShowOmniboxOnTabSwitcher()
                ? toolbarHeight * 2
                : toolbarHeight;
        ExploreSurfaceViewBinder.ViewHolder viewHolder = new ExploreSurfaceViewBinder.ViewHolder(
                parentView, bottomControlsHeight, topMargin, this);
        mPropertyModelChangeProcessor = PropertyModelChangeProcessor.create(
                containerPropertyModel, viewHolder, ExploreSurfaceViewBinder::bind);
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

    // Implements ExploreSurfaceViewBinder.FeedSurfaceDelegate.
    @Override
    public FeedSurfaceCoordinator createFeedSurfaceCoordinator(boolean isIncognito) {
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
                mActivity, null, null, null, exploreSurfaceActionHandler, isIncognito, this);
        // TODO(crbug.com/982018): Customize surface background for incognito and dark mode.
        // TODO(crbug.com/982018): Hide signin promo UI in incognito mode.
    }
}
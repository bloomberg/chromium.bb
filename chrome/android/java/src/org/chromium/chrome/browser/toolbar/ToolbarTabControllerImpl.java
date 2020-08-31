// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.text.TextUtils;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.PageTransition;

/**
 * Implementation of {@link ToolbarTabController}.
 */
public class ToolbarTabControllerImpl implements ToolbarTabController {
    private final Supplier<Tab> mTabSupplier;
    private final Supplier<Boolean> mBottomToolbarVisibilityPredicate;
    private final Supplier<Boolean> mOverrideHomePageSupplier;
    private final Supplier<Profile> mProfileSupplier;
    private final Supplier<BottomControlsCoordinator> mBottomControlsCoordinatorSupplier;
    private final Runnable mOnSuccessRunnable;

    /**
     *
     * @param tabSupplier Supplier for the currently active tab.
     * @param bottomToolbarVisibilityPredicate Predicate that tells us if the bottom toolbar is
     *         visible.
     * @param overrideHomePageSupplier Supplier that returns true if it overrides the default
     *         homepage behavior.
     * @param profileSupplier Supplier for the current profile.
     * @param onSuccessRunnable Runnable that is invoked when the active tab is asked to perform the
     *         corresponding ToolbarTabController action; it is not invoked if the tab cannot
     *         perform the action or for openHompage.
     */
    public ToolbarTabControllerImpl(Supplier<Tab> tabSupplier,
            Supplier<Boolean> bottomToolbarVisibilityPredicate,
            Supplier<Boolean> overrideHomePageSupplier, Supplier<Profile> profileSupplier,
            Supplier<BottomControlsCoordinator> bottomControlsCoordinatorSupplier,
            Runnable onSuccessRunnable) {
        mTabSupplier = tabSupplier;
        mBottomToolbarVisibilityPredicate = bottomToolbarVisibilityPredicate;
        mOverrideHomePageSupplier = overrideHomePageSupplier;
        mProfileSupplier = profileSupplier;
        mBottomControlsCoordinatorSupplier = bottomControlsCoordinatorSupplier;
        mOnSuccessRunnable = onSuccessRunnable;
    }

    @Override
    public boolean back() {
        BottomControlsCoordinator controlsCoordinator = mBottomControlsCoordinatorSupplier.get();
        if (controlsCoordinator != null && controlsCoordinator.onBackPressed()) {
            return true;
        }

        Tab tab = mTabSupplier.get();
        if (tab != null && tab.canGoBack()) {
            tab.goBack();
            mOnSuccessRunnable.run();
            return true;
        }
        return false;
    }

    @Override
    public boolean forward() {
        Tab tab = mTabSupplier.get();
        if (tab != null && tab.canGoForward()) {
            tab.goForward();
            mOnSuccessRunnable.run();
            return true;
        }
        return false;
    }

    @Override
    public void stopOrReloadCurrentTab() {
        Tab currentTab = mTabSupplier.get();
        if (currentTab == null) return;

        if (currentTab.isLoading()) {
            currentTab.stopLoading();
            RecordUserAction.record("MobileToolbarStop");
        } else {
            currentTab.reload();
            RecordUserAction.record("MobileToolbarReload");
        }
        mOnSuccessRunnable.run();
    }

    @Override
    public void openHomepage() {
        RecordUserAction.record("Home");

        if (mBottomToolbarVisibilityPredicate.get()) {
            RecordUserAction.record("MobileBottomToolbarHomeButton");
        } else {
            RecordUserAction.record("MobileTopToolbarHomeButton");
        }

        if (mOverrideHomePageSupplier.get()) {
            return;
        }
        Tab currentTab = mTabSupplier.get();
        if (currentTab == null) return;
        String homePageUrl = HomepageManager.getHomepageUri();
        if (TextUtils.isEmpty(homePageUrl)) {
            homePageUrl = UrlConstants.NTP_URL;
        }
        boolean is_chrome_internal =
                homePageUrl.startsWith(ContentUrlConstants.ABOUT_URL_SHORT_PREFIX)
                || homePageUrl.startsWith(UrlConstants.CHROME_URL_SHORT_PREFIX)
                || homePageUrl.startsWith(UrlConstants.CHROME_NATIVE_URL_SHORT_PREFIX);
        RecordHistogram.recordBooleanHistogram(
                "Navigation.Home.IsChromeInternal", is_chrome_internal);

        recordHomeButtonUseForIPH(homePageUrl);
        currentTab.loadUrl(new LoadUrlParams(homePageUrl, PageTransition.HOME_PAGE));
    }

    /** Record that homepage button was used for IPH reasons */
    private void recordHomeButtonUseForIPH(String homepageUrl) {
        Tab tab = mTabSupplier.get();
        Profile profile = mProfileSupplier.get();
        if (tab == null || profile == null) return;

        Tracker tracker = TrackerFactory.getTrackerForProfile(mProfileSupplier.get());
        tracker.notifyEvent(EventConstants.HOMEPAGE_BUTTON_CLICKED);

        if (NewTabPage.isNTPUrl(homepageUrl)) {
            tracker.notifyEvent(EventConstants.NTP_HOME_BUTTON_CLICKED);
        }
    }
}

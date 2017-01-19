// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.net.Uri;
import android.os.SystemClock;
import android.support.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.favicon.FaviconHelper.IconAvailabilityCallback;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.chrome.browser.ntp.NewTabPage.DestructionObserver;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/**
 * {@link SuggestionsUiDelegate} implementation.
 */
public class SuggestionsUiDelegateImpl implements SuggestionsUiDelegate {
    private final List<DestructionObserver> mDestructionObservers = new ArrayList<>();
    private final SuggestionsSource mSuggestionsSource;
    private final SuggestionsMetricsReporter mSuggestionsMetricsReporter;
    private final SuggestionsNavigationDelegate mSuggestionsNavigationDelegate;

    private final Profile mProfile;

    private final Tab mTab;

    private FaviconHelper mFaviconHelper;
    private LargeIconBridge mLargeIconBridge;

    private boolean mIsDestroyed;

    public SuggestionsUiDelegateImpl(SuggestionsSource suggestionsSource,
            SuggestionsMetricsReporter metricsReporter,
            SuggestionsNavigationDelegate navigationDelegate, Profile profile, Tab currentTab) {
        mSuggestionsSource = suggestionsSource;
        mSuggestionsMetricsReporter = metricsReporter;
        mSuggestionsNavigationDelegate = navigationDelegate;

        mProfile = profile;
        mTab = currentTab;
    }

    @Override
    public void getLocalFaviconImageForURL(
            String url, int size, FaviconImageCallback faviconCallback) {
        if (mIsDestroyed) return;
        getFaviconHelper().getLocalFaviconImageForURL(mProfile, url, size, faviconCallback);
    }

    @Override
    public void getLargeIconForUrl(String url, int size, LargeIconCallback callback) {
        if (mIsDestroyed) return;
        getLargeIconBridge().getLargeIconForUrl(url, size, callback);
    }

    @Override
    public void ensureIconIsAvailable(String pageUrl, String iconUrl, boolean isLargeIcon,
            boolean isTemporary, IconAvailabilityCallback callback) {
        if (mIsDestroyed) return;
        getFaviconHelper().ensureIconIsAvailable(mProfile, mTab.getWebContents(), pageUrl, iconUrl,
                isLargeIcon, isTemporary, callback);
    }

    @Override
    public void getUrlsAvailableOffline(
            Set<String> pageUrls, final Callback<Set<String>> callback) {
        final Set<String> urlsAvailableOffline = new HashSet<>();
        if (mIsDestroyed || !isNtpOfflinePagesEnabled()) {
            callback.onResult(urlsAvailableOffline);
            return;
        }

        HashSet<String> urlsToCheckForOfflinePage = new HashSet<>();

        for (String pageUrl : pageUrls) {
            if (isLocalUrl(pageUrl)) {
                urlsAvailableOffline.add(pageUrl);
            } else {
                urlsToCheckForOfflinePage.add(pageUrl);
            }
        }

        final long offlineQueryStartTime = SystemClock.elapsedRealtime();

        OfflinePageBridge offlinePageBridge = OfflinePageBridge.getForProfile(mProfile);

        // TODO(dewittj): Remove this code by making the NTP badging available after the NTP is
        // fully loaded.
        if (offlinePageBridge == null || !offlinePageBridge.isOfflinePageModelLoaded()) {
            // Posting a task to avoid potential re-entrancy issues.
            ThreadUtils.postOnUiThread(new Runnable() {
                @Override
                public void run() {
                    callback.onResult(urlsAvailableOffline);
                }
            });
            return;
        }

        offlinePageBridge.checkPagesExistOffline(
                urlsToCheckForOfflinePage, new Callback<Set<String>>() {
                    @Override
                    public void onResult(Set<String> urlsWithOfflinePages) {
                        urlsAvailableOffline.addAll(urlsWithOfflinePages);
                        callback.onResult(urlsAvailableOffline);
                        RecordHistogram.recordTimesHistogram("NewTabPage.OfflineUrlsLoadTime",
                                SystemClock.elapsedRealtime() - offlineQueryStartTime,
                                TimeUnit.MILLISECONDS);
                    }
                });
    }

    @Override
    public SuggestionsSource getSuggestionsSource() {
        return mSuggestionsSource;
    }

    @Nullable
    @Override
    public SuggestionsMetricsReporter getMetricsReporter() {
        return mSuggestionsMetricsReporter;
    }

    @Nullable
    @Override
    public SuggestionsNavigationDelegate getNavigationDelegate() {
        return mSuggestionsNavigationDelegate;
    }

    @Override
    public void addDestructionObserver(DestructionObserver destructionObserver) {
        mDestructionObservers.add(destructionObserver);
    }

    /** Invalidates the delegate and calls the registered destruction observers. */
    public void onDestroy() {
        assert !mIsDestroyed;

        for (DestructionObserver observer : mDestructionObservers) observer.onDestroy();

        if (mFaviconHelper != null) {
            mFaviconHelper.destroy();
            mFaviconHelper = null;
        }
        if (mLargeIconBridge != null) {
            mLargeIconBridge.destroy();
            mLargeIconBridge = null;
        }
        mIsDestroyed = true;
    }

    /**
     * Utility method to lazily create the {@link FaviconHelper}, and avoid unnecessary native
     * calls in tests.
     */
    private FaviconHelper getFaviconHelper() {
        assert !mIsDestroyed;
        if (mFaviconHelper == null) mFaviconHelper = new FaviconHelper();
        return mFaviconHelper;
    }

    /**
     * Utility method to lazily create the {@link LargeIconBridge}, and avoid unnecessary native
     * calls in tests.
     */
    private LargeIconBridge getLargeIconBridge() {
        assert !mIsDestroyed;
        if (mLargeIconBridge == null) mLargeIconBridge = new LargeIconBridge(mProfile);
        return mLargeIconBridge;
    }

    private boolean isNtpOfflinePagesEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.NTP_OFFLINE_PAGES_FEATURE_NAME);
    }

    private boolean isLocalUrl(String url) {
        return "file".equals(Uri.parse(url).getScheme());
    }
}

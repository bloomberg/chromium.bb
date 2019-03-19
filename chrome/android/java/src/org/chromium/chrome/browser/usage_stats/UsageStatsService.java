// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import android.app.Activity;

import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.ArrayList;
import java.util.List;

/**
 * Public interface for all usage stats related functionality. All calls to instances of
 * UsageStatsService must be made on the UI thread.
 */
public class UsageStatsService {
    private static UsageStatsService sInstance;

    private EventTracker mEventTracker;
    private SuspensionTracker mSuspensionTracker;
    private TokenTracker mTokenTracker;
    private UsageStatsBridge mBridge;

    private DigitalWellbeingClient mClient;
    private boolean mOptInState;

    /** Get the global instance of UsageStatsService */
    public static UsageStatsService getInstance() {
        if (sInstance == null) {
            sInstance = new UsageStatsService();
        }

        return sInstance;
    }

    @VisibleForTesting
    UsageStatsService() {
        Profile profile = Profile.getLastUsedProfile().getOriginalProfile();
        mBridge = new UsageStatsBridge(profile);
        mEventTracker = new EventTracker(mBridge);
        mSuspensionTracker = new SuspensionTracker(mBridge);
        mTokenTracker = new TokenTracker(mBridge);

        mOptInState = getOptInState();
        mClient = AppHooks.get().createDigitalWellbeingClient();
    }

    /**
     * Create a {@link PageViewObserver} for the given tab model selector and activity.
     * @param tabModelSelector The tab model selector that should be used to get the current tab
     *         model.
     * @param activity The activity in which page view events are occuring.
     */
    public PageViewObserver createPageViewObserver(
            TabModelSelector tabModelSelector, Activity activity) {
        ThreadUtils.assertOnUiThread();
        return new PageViewObserver(
                activity, tabModelSelector, mEventTracker, mTokenTracker, mSuspensionTracker);
    }

    /** @return Whether the user has authorized DW to access usage stats data. */
    public boolean getOptInState() {
        ThreadUtils.assertOnUiThread();
        PrefServiceBridge prefServiceBridge = PrefServiceBridge.getInstance();
        return prefServiceBridge.getBoolean(Pref.USAGE_STATS_ENABLED);
    }

    /** Sets the user's opt in state. */
    public void setOptInState(boolean state) {
        ThreadUtils.assertOnUiThread();
        PrefServiceBridge prefServiceBridge = PrefServiceBridge.getInstance();
        prefServiceBridge.setBoolean(Pref.USAGE_STATS_ENABLED, state);

        if (mOptInState == state) return;

        mOptInState = state;
        mClient.notifyOptInStateChange(mOptInState);
    }

    /** Query for all events that occurred in the half-open range [start, end) */
    public Promise<List<WebsiteEvent>> queryWebsiteEventsAsync(long start, long end) {
        ThreadUtils.assertOnUiThread();
        return mEventTracker.queryWebsiteEvents(start, end);
    }

    /** Get all tokens that are currently being tracked. */
    public Promise<List<String>> getAllTrackedTokensAsync() {
        ThreadUtils.assertOnUiThread();
        return mTokenTracker.getAllTrackedTokens();
    }

    /**
     * Start tracking a full-qualified domain name(FQDN), returning the token used to identify it.
     * If the FQDN is already tracked, this will return the existing token.
     */
    public Promise<String> startTrackingWebsiteAsync(String fqdn) {
        ThreadUtils.assertOnUiThread();
        return mTokenTracker.startTrackingWebsite(fqdn);
    }

    /**
     * Stops tracking the site associated with the given token.
     * If the token was not associated with a site, this does nothing.
     */
    public Promise<Void> stopTrackingTokenAsync(String token) {
        ThreadUtils.assertOnUiThread();
        return mTokenTracker.stopTrackingToken(token);
    }

    /**
     * Suspend or unsuspend every site in FQDNs, depending on the truthiness of <c>suspended</c>.
     */
    public Promise<Void> setWebsitesSuspendedAsync(List<String> fqdns, boolean suspended) {
        ThreadUtils.assertOnUiThread();
        return mSuspensionTracker.setWebsitesSuspended(fqdns, suspended);
    }

    /** @return all the sites that are currently suspended. */
    public Promise<List<String>> getAllSuspendedWebsitesAsync() {
        ThreadUtils.assertOnUiThread();
        return mSuspensionTracker.getAllSuspendedWebsites();
    }

    // The below methods are dummies that are only being retained to avoid breaking the downstream
    // build. TODO(pnoland): remove these once the downstream change that converts to using promises
    // lands.
    public List<WebsiteEvent> queryWebsiteEvents(long start, long end) {
        return new ArrayList<>();
    }

    public List<String> getAllTrackedTokens() {
        return new ArrayList<>();
    }

    public String startTrackingWebsite(String fqdn) {
        return "1";
    }

    public void stopTrackingToken(String token) {
        return;
    }

    public void setWebsitesSuspended(List<String> fqdns, boolean suspended) {
        return;
    }

    public List<String> getAllSuspendedWebsites() {
        return new ArrayList<>();
    }
}
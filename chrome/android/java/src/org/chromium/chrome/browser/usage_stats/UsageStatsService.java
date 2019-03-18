// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import android.annotation.SuppressLint;
import android.app.Activity;

import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.usage_stats.WebsiteEventProtos.Timestamp;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;

/**
 * Public interface for all usage stats related functionality. All calls to instances of
 * UsageStatsService must be made on the UI thread.
 */
@SuppressLint("NewApi")
public class UsageStatsService {
    private static final String TAG = "UsageStatsService";

    private static UsageStatsService sInstance;

    private EventTracker mEventTracker;
    private SuspensionTracker mSuspensionTracker;
    private TokenTracker mTokenTracker;
    private UsageStatsBridge mBridge;
    private List<Runnable> mEventRunnables;
    private List<Runnable> mSuspensionRunnables;
    private List<Runnable> mTokenRunnables;

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

        mEventRunnables = new ArrayList<>();
        mSuspensionRunnables = new ArrayList<>();
        mTokenRunnables = new ArrayList<>();
        mBridge.getAllEvents(this::onGetAllEvents);
        mBridge.getAllSuspensions(this::onGetAllSuspensions);
        mBridge.getAllTokenMappings(this::onGetAllTokenMappings);
        // TODO(pnoland): listen for preference changes so we can notify DW.
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
        return new PageViewObserver(activity, tabModelSelector, this);
    }

    /** @return Whether the user has authorized Digital Wellbeing(DW) to access usage stats data. */
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
    }

    /**
     * @return the token for a given FQDN. Returns null if we're not tracking that FQDN or if we
     * haven't loaded token mappings from disk yet.
     */
    public String getTokenForFqdn(String fqdn) {
        ThreadUtils.assertOnUiThread();
        if (mTokenTracker == null) {
            return null;
        }

        return mTokenTracker.getTokenForFqdn(fqdn);
    }

    // The below methods with suffix "Async" return a promise instead of the actual type requested.
    // If data has been loaded from disk, these promises fulfill immediately. If not, a runnable
    // that fulfills the promise by re-calling the appropriate function is added to a queue.
    // Runnables in the queue are invoked once the load from disk is complete.
    // TODO(pnoland): consider removing the Async suffix once the dummy synchronous methods have
    // been removed.

    /** Query for all events that occurred in the half-open range [start, end) */
    public Promise<List<WebsiteEvent>> queryWebsiteEventsAsync(long start, long end) {
        ThreadUtils.assertOnUiThread();
        final Promise<List<WebsiteEvent>> promise = new Promise<>();

        if (mEventTracker != null) {
            promise.fulfill(mEventTracker.queryWebsiteEvents(start, end));
        } else {
            mEventRunnables.add(
                    () -> promise.fulfill(this.queryWebsiteEventsAsync(start, end).getResult()));
        }

        return promise;
    }

    /** Add a WebsiteEvent to the record of a user's activity. */
    public Promise<Void> addWebsiteEventAsync(WebsiteEvent event) {
        ThreadUtils.assertOnUiThread();
        final Promise<Void> promise = new Promise<>();

        if (mEventTracker != null) {
            mEventTracker.addWebsiteEvent(event);
            List<WebsiteEventProtos.WebsiteEvent> eventList =
                    Arrays.asList(getProtoEventFromNativeEvent(event));
            mBridge.addEvents(eventList, this::onDataPersisted);
        } else {
            mEventRunnables.add(
                    () -> promise.fulfill(this.addWebsiteEventAsync(event).getResult()));
        }

        return promise;
    }

    /** Get all tokens that are currently being tracked. */
    public Promise<List<String>> getAllTrackedTokensAsync() {
        ThreadUtils.assertOnUiThread();
        final Promise<List<String>> promise = new Promise<>();

        if (mTokenTracker != null) {
            promise.fulfill(mTokenTracker.getAllTrackedTokens());
        } else {
            mTokenRunnables.add(() -> promise.fulfill(this.getAllTrackedTokensAsync().getResult()));
        }

        return promise;
    }

    /**
     * Start tracking a full-qualified domain name(FQDN), returning the token used to identify it.
     * If the FQDN is already tracked, this will return the existing token.
     */
    public Promise<String> startTrackingWebsiteAsync(String fqdn) {
        ThreadUtils.assertOnUiThread();
        final Promise<String> promise = new Promise<>();

        if (mTokenTracker != null) {
            String token = mTokenTracker.startTrackingWebsite(fqdn);
            mBridge.setTokenMappings(mTokenTracker.getAllTokenMappings(), this::onDataPersisted);
            promise.fulfill(token);
        } else {
            mTokenRunnables.add(
                    () -> promise.fulfill(this.startTrackingWebsiteAsync(fqdn).getResult()));
        }

        return promise;
    }

    /**
     * Stops tracking the site associated with the given token.
     * If the token was not associated with a site, this does nothing.
     */
    public Promise<Void> stopTrackingTokenAsync(String token) {
        ThreadUtils.assertOnUiThread();
        final Promise<Void> promise = new Promise<>();

        if (mTokenTracker != null) {
            mTokenTracker.stopTrackingToken(token);
            mBridge.setTokenMappings(mTokenTracker.getAllTokenMappings(), this::onDataPersisted);
            promise.fulfill(null);
        } else {
            mTokenRunnables.add(
                    () -> promise.fulfill(this.stopTrackingTokenAsync(token).getResult()));
        }

        return promise;
    }

    /**
     * Suspend or unsuspend every site in FQDNs, depending on the truthiness of <c>suspended</c>.
     */
    public Promise<Void> setWebsitesSuspendedAsync(List<String> fqdns, boolean suspended) {
        ThreadUtils.assertOnUiThread();
        final Promise<Void> promise = new Promise<>();

        if (mSuspensionTracker != null) {
            mSuspensionTracker.setWebsitesSuspended(fqdns, suspended);
            // new String[1] lets the JVM return a String[] instead of Object[], which it does by
            // default due to type erasure.
            mBridge.setSuspensions(
                    mSuspensionTracker.getAllSuspendedWebsites().toArray(new String[1]),
                    this::onDataPersisted);
            promise.fulfill(null);
        } else {
            mSuspensionRunnables.add(
                    ()
                            -> promise.fulfill(
                                    this.setWebsitesSuspendedAsync(fqdns, suspended).getResult()));
        }

        return promise;
    }

    /** @return all the sites that are currently suspended. */
    public Promise<List<String>> getAllSuspendedWebsitesAsync() {
        ThreadUtils.assertOnUiThread();
        final Promise<List<String>> promise = new Promise<>();

        if (mSuspensionTracker != null) {
            promise.fulfill(mSuspensionTracker.getAllSuspendedWebsites());
        } else {
            mSuspensionRunnables.add(
                    () -> promise.fulfill(this.getAllSuspendedWebsitesAsync().getResult()));
        }

        return promise;
    }

    /** @return whether the given site is suspended. */
    public Promise<Boolean> isWebsiteSuspendedAsync(String fqdn) {
        ThreadUtils.assertOnUiThread();
        final Promise<Boolean> promise = new Promise<>();

        if (mSuspensionTracker != null) {
            promise.fulfill(mSuspensionTracker.isWebsiteSuspended(fqdn));
        } else {
            mSuspensionRunnables.add(
                    () -> promise.fulfill(this.isWebsiteSuspendedAsync(fqdn).getResult()));
        }

        return promise;
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

    public boolean isWebsiteSuspended(String fqdn) {
        return false;
    }

    private WebsiteEventProtos.WebsiteEvent getProtoEventFromNativeEvent(WebsiteEvent nativeEvent) {
        return WebsiteEventProtos.WebsiteEvent.newBuilder()
                .setFqdn(nativeEvent.getFqdn())
                .setTimestamp(getProtoTimestampFromNativeTimestamp(nativeEvent.getTimestamp()))
                .setType(getProtoEventTypeFromNativeEventType(nativeEvent.getType()))
                .build();
    }

    private Timestamp getProtoTimestampFromNativeTimestamp(long nativeTimestampMs) {
        return Timestamp.newBuilder()
                .setSeconds(nativeTimestampMs / 1000)
                .setNanos((int) ((nativeTimestampMs % 1000) * 1000000))
                .build();
    }

    private WebsiteEventProtos.WebsiteEvent.EventType getProtoEventTypeFromNativeEventType(
            @WebsiteEvent.EventType int nativeEventType) {
        switch (nativeEventType) {
            case WebsiteEvent.EventType.START:
                return WebsiteEventProtos.WebsiteEvent.EventType.START_BROWSING;
            case WebsiteEvent.EventType.STOP:
                return WebsiteEventProtos.WebsiteEvent.EventType.STOP_BROWSING;
            default:
                return WebsiteEventProtos.WebsiteEvent.EventType.UNKNOWN;
        }
    }

    private long getNativeTimestampFromProtoTimestamp(Timestamp protoTimestamp) {
        return protoTimestamp.getSeconds() * 1000 + protoTimestamp.getNanos() / 1000;
    }

    private void onDataPersisted(boolean didSucceed) {
        if (!didSucceed) {
            Log.e(TAG, "Writing to usage stats persistence failed");
        }
    }

    private void onGetAllEvents(List<WebsiteEventProtos.WebsiteEvent> protoEvents) {
        List<WebsiteEvent> events = new ArrayList<>(protoEvents.size());
        for (WebsiteEventProtos.WebsiteEvent protoEvent : protoEvents) {
            events.add(new WebsiteEvent(
                    getNativeTimestampFromProtoTimestamp(protoEvent.getTimestamp()),
                    protoEvent.getFqdn(), protoEvent.getType().getNumber()));
        }

        mEventTracker = new EventTracker(events);
        for (Runnable runnable : mEventRunnables) {
            runnable.run();
        }

        mEventRunnables.clear();
    }

    private void onGetAllSuspensions(List<String> result) {
        mSuspensionTracker = new SuspensionTracker(result);
        for (Runnable runnable : mSuspensionRunnables) {
            runnable.run();
        }

        mSuspensionRunnables.clear();
    }

    private void onGetAllTokenMappings(Map<String, String> result) {
        mTokenTracker = new TokenTracker(result);
        for (Runnable runnable : mTokenRunnables) {
            runnable.run();
        }

        mTokenRunnables.clear();
    }
}
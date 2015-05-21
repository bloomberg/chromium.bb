// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.content.Context;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchSelectionController.SelectionType;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.NetworkPredictionOptions;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;

import java.net.URL;

import javax.annotation.Nullable;


/**
 * Handles policy decisions for the {@code ContextualSearchManager}.
 */
class ContextualSearchPolicy {
    private static final int PROMO_TAPS_NOT_LIMITED = -1;
    private static final int PROMO_TAPS_DISABLED_BIAS = -2;

    private static ContextualSearchPolicy sInstance;

    private final ChromePreferenceManager mPreferenceManager;

    // Members used only for testing purposes.
    private boolean mDidOverrideDecidedStateForTesting;
    private boolean mDecidedStateForTesting;
    private boolean mDidResetTapCounters;

    static ContextualSearchPolicy getInstance(Context context) {
        if (sInstance == null) {
            sInstance = new ContextualSearchPolicy(context);
        }
        return sInstance;
    }

    /**
     * @param context The Android Context.
     */
    ContextualSearchPolicy(Context context) {
        mPreferenceManager = ChromePreferenceManager.getInstance(context);
    }

    // TODO(donnd): Consider adding a test-only constructor that uses dependency injection of a
    // preference manager and PrefServiceBridge.  Currently this is not possible because the
    // PrefServiceBridge is final.

    /**
     * @return The number of additional times to show the promo on tap, 0 if it should not be shown,
     *         or a negative value if the counter has been disabled.
     */
    int getPromoTapsRemaining() {
        if (ContextualSearchFieldTrial.isPromoLimitedByTapCounts()) {
            int count = mPreferenceManager.getContextualSearchTapTriggeredPromoCount();

            // Return a negative value if opt-out promo counter has been disabled.
            if (isOptOutPromoAvailable() && !isOptOutPromoCounterEnabled(count)) return count;

            int limit = ContextualSearchFieldTrial.getPromoTapTriggeredLimit();
            if (limit >= 0) return Math.max(0, limit - count);
        }

        return PROMO_TAPS_NOT_LIMITED;
    }

    /**
     * @return Whether a Tap gesture is currently supported as a trigger for the feature.
     */
    boolean isTapSupported() {
        if (!isUserUndecided()) return true;

        if (ContextualSearchFieldTrial.isPromoLongpressTriggeredOnly()) return false;

        return getPromoTapsRemaining() != 0;
    }

    /**
     * @return whether or not the Contextual Search Result should be preloaded before the user
     *         explicitly interacts with the feature.
     */
    boolean shouldPrefetchSearchResult(boolean isTapTriggered) {
        if (PrefServiceBridge.getInstance().getNetworkPredictionOptions()
                == NetworkPredictionOptions.NETWORK_PREDICTION_NEVER) {
            return false;
        }

        if (isTapPrefetchBeyondTheLimit()) return false;

        // If we're not resolving the tap due to the tap limit, we should not preload either.
        if (isTapResolveBeyondTheLimit()) return false;

        // We never preload on long-press so users can cut & paste without hitting the servers.
        return isTapTriggered;
    }

    /**
     * Returns whether the previous tap (the tap last counted) should resolve.
     * @return Whether the previous tap should resolve.
     */
    boolean shouldPreviousTapResolve(@Nullable URL url) {
        if (isTapResolveBeyondTheLimit()) {
            return false;
        }

        if (isOptOutPromoAvailable()) {
            return isBasePageHTTP(url);
        }

        return true;
    }

    /**
     * Returns whether surrounding context can be accessed by other systems or not.
     * @baseContentViewUrl The URL of the base page.
     * @return Whether surroundings are available.
     */
    boolean canSendSurroundings(@Nullable URL baseContentViewUrl) {
        if (isUserUndecided()) return false;

        if (isOptOutPromoAvailable()) {
            return isBasePageHTTP(baseContentViewUrl);
        }

        return true;
    }

    /**
     * @return Whether the Opt-out promo is available to be shown in any panel.
     */
    boolean isOptOutPromoAvailable() {
        return ContextualSearchFieldTrial.isPromoOptOut() && isUserUndecided();
    }

    /**
     * @return Whether the classic opt-in promo is available.
     */
    protected boolean isOptInPromoAvailable() {
        return false;
    }

    /**
     * Registers that a tap has taken place by incrementing tap-tracking counters.
     */
    void registerTap() {
        if (isOptInPromoAvailable() || isOptOutPromoAvailable()) {
            int count = mPreferenceManager.getContextualSearchTapTriggeredPromoCount();
            // Bump the counter only when it is still enabled.
            if (isOptInPromoAvailable() || isOptOutPromoCounterEnabled(count)) {
                mPreferenceManager.setContextualSearchTapTriggeredPromoCount(++count);
            }
        }
        if (isTapLimited()) {
            int count = mPreferenceManager.getContextualSearchTapCount();
            mPreferenceManager.setContextualSearchTapCount(++count);
        }
    }

    /**
     * Resets all the "tap" counters.
     */
    void resetTapCounters() {
        // Always completely reset the tap counters, since tests push beyond limits: this
        // would affect subsequent tests unless they can reset without having a limit.
        mPreferenceManager.setContextualSearchTapCount(0);

        // Disable the "promo tap" counter, but only if we're using the Opt-out onboarding.
        // For Opt-in, we never disable the promo tap counter.
        if (isOptOutPromoAvailable()) disableOptOutPromoCounter();
        mDidResetTapCounters = true;
    }

    @VisibleForTesting
    void overrideDecidedStateForTesting(boolean decidedState) {
        mDidOverrideDecidedStateForTesting = true;
        mDecidedStateForTesting = decidedState;
    }

    @VisibleForTesting
    boolean didResetTapCounters() {
        return mDidResetTapCounters;
    }

    /**
     * @return Whether a verbatim request should be made for the given base page, assuming there
     *         is no exiting request.
     */
    boolean shouldCreateVerbatimRequest(ContextualSearchSelectionController controller,
            @Nullable URL basePageUrl) {
        // TODO(donnd): refactor to make the controller a member of this class?
        return (controller.getSelectedText() != null
                && (controller.getSelectionType() == SelectionType.LONG_PRESS
                || (controller.getSelectionType() == SelectionType.TAP
                        && !shouldPreviousTapResolve(basePageUrl))));
    }

    /**
     * Determines whether an error from a search term resolution request should
     * be shown to the user, or not.
     */
    boolean shouldShowErrorCodeInBar() {
        // Builds with lots of real users should not see raw error codes.
        return !(ChromeVersionInfo.isStableBuild() || ChromeVersionInfo.isBetaBuild());
    }

    // --------------------------------------------------------------------------------------------
    // Opt-out style Promo counter
    //
    // The Opt-out style promo tap counter needs to do two things:
    // 1) Count Taps that trigger the promo, so they can be limited.
    // 2) Support a "disabled" state; when the user opens the panel then Taps trigger from then on.
    // We use a single persistent setting to record both meanings by using a negative value to
    // indicate disabled.
    //
    // TODO(donnd): make a separate class for this kind of counter.
    // --------------------------------------------------------------------------------------------

    /**
     * Determines if the given Opt-out style promo counter represents a count of promo taps in
     * the enabled state.
     * @param counter The persistent counter value to consider.
     * @return Whether the given counter is enabled.
     */
    private boolean isOptOutPromoCounterEnabled(int counter) {
        return counter >= 0;
    }

    /**
     * Generates an equivalent counter value with the enabled state opposite of the given value.
     * @param count The current value of the counter.
     * @return The equivalent value in with its enabled/disabled state toggled.
     */
    private int toggleOptOutPromoCounterEnabled(int count) {
        return PROMO_TAPS_DISABLED_BIAS - count;
    }

    /**
     * Disables the Opt-out promo counter, unless it is already disabled.
     */
    private void disableOptOutPromoCounter() {
        int count = mPreferenceManager.getContextualSearchTapTriggeredPromoCount();
        if (isOptOutPromoCounterEnabled(count)) {
            count = toggleOptOutPromoCounterEnabled(count);
            mPreferenceManager.setContextualSearchTapTriggeredPromoCount(count);
        }
    }

    // --------------------------------------------------------------------------------------------
    // Private helpers.
    // --------------------------------------------------------------------------------------------

    /**
     * @return Whether a promo is needed because the user is still undecided
     *         on enabling or disabling the feature.
     */
    private boolean isUserUndecided() {
        // TODO(donnd) use dependency injection for the PrefServiceBridge instead!
        if (mDidOverrideDecidedStateForTesting) return !mDecidedStateForTesting;

        return PrefServiceBridge.getInstance().isContextualSearchUninitialized();
    }

    /**
     * @param url The URL of the base page.
     * @return Whether the given content view is for an HTTP page.
     */
    private boolean isBasePageHTTP(@Nullable URL url) {
        // We shouldn't be checking HTTP unless we're in the opt-out promo which
        // treats HTTP differently.
        assert ContextualSearchFieldTrial.isPromoOptOut();
        return url != null && "http".equals(url.getProtocol());
    }

    /**
     * @return Whether the tap resolve limit has been exceeded.
     */
    private boolean isTapResolveBeyondTheLimit() {
        return isTapResolveLimited()
                && mPreferenceManager.getContextualSearchTapCount() > getTapResolveLimit();
    }

    /**
     * @return Whether the tap resolve limit has been exceeded.
     */
    private boolean isTapPrefetchBeyondTheLimit() {
        return isTapPrefetchLimited()
                && mPreferenceManager.getContextualSearchTapCount() > getTapPrefetchLimit();
    }

    /**
     * Whether taps for decided users are limited, either for prefetch or resolve.
     */
    private boolean isTapLimited() {
        return isTapPrefetchLimited() || isTapResolveLimited();
    }

    /**
     * @return Whether a tap gesture is resolve-limited.
     */
    private boolean isTapResolveLimited() {
        return isUserUndecided()
                ? isTapResolveLimitedForUndecided()
                : isTapResolveLimitedForDecided();
    }

    /**
     * @return Whether a tap gesture is resolve-limited.
     */
    private boolean isTapPrefetchLimited() {
        return isUserUndecided()
                ? isTapPrefetchLimitedForUndecided()
                : isTapPrefetchLimitedForDecided();
    }

    /**
     * @return The limit of the number of taps to prefetch.
     */
    private int getTapPrefetchLimit() {
        return isUserUndecided()
                ? ContextualSearchFieldTrial.getTapPrefetchLimitForUndecided()
                : ContextualSearchFieldTrial.getTapPrefetchLimitForDecided();
    }

    /**
     * @return The limit of the number of taps to resolve using search term resolution.
     */
    private int getTapResolveLimit() {
        return isUserUndecided()
                ? ContextualSearchFieldTrial.getTapResolveLimitForUndecided()
                : ContextualSearchFieldTrial.getTapResolveLimitForDecided();
    }

    /**
     * @return Whether Search Term Resolution in response to a Tap gesture is limited for decided
     *         users.
     */
    private boolean isTapResolveLimitedForDecided() {
        return ContextualSearchFieldTrial.getTapResolveLimitForDecided()
                != ContextualSearchFieldTrial.UNLIMITED_TAPS;
    }

    /**
     * @return Whether prefetch in response to a Tap gesture is limited for decided users.
     */
    private boolean isTapPrefetchLimitedForDecided() {
        return ContextualSearchFieldTrial.getTapPrefetchLimitForDecided()
                != ContextualSearchFieldTrial.UNLIMITED_TAPS;
    }

    /**
     * @return Whether Search Term Resolution in response to a Tap gesture is limited for undecided
     *         users.
     */
    private boolean isTapResolveLimitedForUndecided() {
        return ContextualSearchFieldTrial.getTapResolveLimitForUndecided()
                != ContextualSearchFieldTrial.UNLIMITED_TAPS;
    }

    /**
     * @return Whether prefetch in response to a Tap gesture is limited for undecided users.
     */
    private boolean isTapPrefetchLimitedForUndecided() {
        return ContextualSearchFieldTrial.getTapPrefetchLimitForUndecided()
                != ContextualSearchFieldTrial.UNLIMITED_TAPS;
    }
}

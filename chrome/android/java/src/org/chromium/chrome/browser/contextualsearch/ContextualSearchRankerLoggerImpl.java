// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import org.chromium.chrome.browser.contextualsearch.ContextualSearchRankerLogger.Feature;

import java.net.URL;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * Implements the UMA logging for Ranker that's used for Contextual Search Tap Suppression.
 */
public class ContextualSearchRankerLoggerImpl implements ContextualSearchRankerLogger {
    private static final String TAG = "ContextualSearch";

    // Names for our features.
    private static final Map<Feature, String> FEATURE_NAMES;
    static {
        Map<Feature, String> names = new HashMap<Feature, String>();
        names.put(Feature.OUTCOME_WAS_PANEL_OPENED, "OutcomeWasPanelOpened");
        names.put(Feature.OUTCOME_WAS_QUICK_ACTION_CLICKED, "OutcomeWasQuickActionClicked");
        names.put(Feature.OUTCOME_WAS_QUICK_ANSWER_SEEN, "OutcomeWasQuickAnswerSeen");
        names.put(Feature.DURATION_AFTER_SCROLL_MS, "DurationAfterScrollMs");
        names.put(Feature.SCREEN_TOP_DPS, "ScreenTopDps");
        names.put(Feature.WAS_SCREEN_BOTTOM, "WasScreenBottom");
        names.put(Feature.PREVIOUS_WEEK_IMPRESSIONS_COUNT, "PreviousWeekImpressionsCount");
        names.put(Feature.PREVIOUS_WEEK_CTR_PERCENT, "PreviousWeekCtrPercent");
        names.put(Feature.PREVIOUS_28DAY_IMPRESSIONS_COUNT, "Previous28DayImpressionsCount");
        names.put(Feature.PREVIOUS_28DAY_CTR_PERCENT, "Previous28DayCtrPercent");
        FEATURE_NAMES = Collections.unmodifiableMap(names);
    }

    // Pointer to the native instance of this class.
    private long mNativePointer;

    // Whether logging for the current URL has been setup.
    private boolean mIsLoggingReadyForUrl;

    // URL of the base page that the log data is associated with.
    private URL mBasePageUrl;

    // Whether inference has already occurred for this interaction (and calling #logFeature is no
    // longer allowed).
    private boolean mHasInferenceOccurred;

    // Whether the UI was suppressed.
    private boolean mWasUiSuppressionInfered;

    // Map that accumulates all of the Features to log for a specific user-interaction.
    private Map<Feature, Object> mFeaturesToLog;

    /**
     * Constructs a Ranker Logger and associated native implementation to write Contextual Search
     * ML data to Ranker.
     */
    public ContextualSearchRankerLoggerImpl() {
        if (isEnabled()) mNativePointer = nativeInit();
    }

    /**
     * This method should be called to clean up storage when an instance of this class is
     * no longer in use.  The nativeDestroy will call the destructor on the native instance.
     */
    void destroy() {
        if (isEnabled()) {
            assert mNativePointer != 0;
            writeLogAndReset();
            nativeDestroy(mNativePointer);
            mNativePointer = 0;
        }
        mIsLoggingReadyForUrl = false;
    }

    @Override
    public void setupLoggingForPage(URL basePageUrl) {
        mIsLoggingReadyForUrl = true;
        mBasePageUrl = basePageUrl;
        mHasInferenceOccurred = false;
    }

    @Override
    public void logFeature(Feature feature, Object value) {
        assert mIsLoggingReadyForUrl : "mIsLoggingReadyForUrl false.";
        assert !mHasInferenceOccurred;
        if (!isEnabled()) return;

        logInternal(feature, value);
    }

    @Override
    public void logOutcome(Feature feature, Object value) {
        if (!isEnabled()) return;

        // Since the panel can be closed at any time, we might try to log that outcome immediately.
        if (!mIsLoggingReadyForUrl) return;

        logInternal(feature, value);
    }

    @Override
    public boolean inferUiSuppression() {
        mHasInferenceOccurred = true;
        // TODO(donnd): actually run the Ranker model and register its recommendation here!
        mWasUiSuppressionInfered = false;
        // TODO(donnd): actually return the recommendation so it can be acted upon!
        return false;
    }

    @Override
    public boolean wasUiSuppressionInfered() {
        return mWasUiSuppressionInfered;
    }

    @Override
    public void reset() {
        mIsLoggingReadyForUrl = false;
        mHasInferenceOccurred = false;
        mFeaturesToLog = null;
        mBasePageUrl = null;
        mWasUiSuppressionInfered = false;
    }

    @Override
    public void writeLogAndReset() {
        // The URL may be null for custom Chrome URIs like chrome://flags.
        if (isEnabled() && mBasePageUrl != null && mFeaturesToLog != null) {
            assert mIsLoggingReadyForUrl;
            nativeSetupLoggingAndRanker(mNativePointer, mBasePageUrl.toString());
            for (Map.Entry<Feature, Object> entry : mFeaturesToLog.entrySet()) {
                logObject(entry.getKey(), entry.getValue());
            }
            nativeWriteLogAndReset(mNativePointer);
        }
        reset();
    }

    /**
     * Logs the given feature/value to the internal map that accumulates an entire record (which can
     * be logged by calling writeLogAndReset).
     * @param feature The feature to log.
     * @param value The value to log.
     */
    private void logInternal(Feature feature, Object value) {
        if (mFeaturesToLog == null) mFeaturesToLog = new HashMap<Feature, Object>();
        mFeaturesToLog.put(feature, value);
    }

    /** Whether actually writing data is enabled.  If not, we may do nothing, or just print. */
    private boolean isEnabled() {
        return !ContextualSearchFieldTrial.isRankerLoggingDisabled();
    }

    /**
     * Logs the given {@link ContextualSearchRankerLogger.Feature} with the given value
     * {@link Object}.
     * @param feature The feature to log.
     * @param value An {@link Object} value to log (must be convertible to a {@code long}).
     */
    private void logObject(Feature feature, Object value) {
        if (value instanceof Boolean) {
            logToNative(feature, ((boolean) value ? 1 : 0));
        } else if (value instanceof Integer) {
            logToNative(feature, Long.valueOf((int) value));
        } else if (value instanceof Long) {
            logToNative(feature, (long) value);
        } else if (value instanceof Character) {
            logToNative(feature, Character.getNumericValue((char) value));
        } else {
            assert false : "Could not log feature to Ranker: " + feature.toString() + " of class "
                           + value.getClass();
        }
    }

    /**
     * Logs to the native instance.  All native logging must go through this bottleneck.
     * @param feature The feature to log.
     * @param value The value to log.
     */
    private void logToNative(Feature feature, long value) {
        nativeLogLong(mNativePointer, getFeatureName(feature), value);
    }

    /**
     * @return The name of the given feature.
     */
    private String getFeatureName(Feature feature) {
        return FEATURE_NAMES.get(feature);
    }

    // ============================================================================================
    // Native methods.
    // ============================================================================================
    private native long nativeInit();
    private native void nativeDestroy(long nativeContextualSearchRankerLoggerImpl);
    private native void nativeLogLong(
            long nativeContextualSearchRankerLoggerImpl, String featureString, long value);
    private native void nativeSetupLoggingAndRanker(
            long nativeContextualSearchRankerLoggerImpl, String basePageUrl);
    private native void nativeWriteLogAndReset(long nativeContextualSearchRankerLoggerImpl);
}

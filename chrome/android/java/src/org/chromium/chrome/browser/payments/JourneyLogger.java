// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.JNINamespace;

/**
 * A class used to record journey metrics for the Payment Request feature.
 */
@JNINamespace("payments")
public class JourneyLogger {
    // Note: The constants should always be in sync with those in the
    // components/payments/core/journey_logger.h file.
    // The index of the different sections of a Payment Request. Used to record journey stats.
    public static final int SECTION_CONTACT_INFO = 0;
    public static final int SECTION_CREDIT_CARDS = 1;
    public static final int SECTION_SHIPPING_ADDRESS = 2;
    public static final int SECTION_MAX = 3;

    // For the CanMakePayment histograms.
    public static final int CAN_MAKE_PAYMENT_USED = 0;
    public static final int CAN_MAKE_PAYMENT_NOT_USED = 1;
    public static final int CAN_MAKE_PAYMENT_USE_MAX = 2;

    // Used to log different parameters' effect on whether the transaction was completed.
    public static final int COMPLETION_STATUS_COMPLETED = 0;
    public static final int COMPLETION_STATUS_USER_ABORTED = 1;
    public static final int COMPLETION_STATUS_OTHER_ABORTED = 2;
    public static final int COMPLETION_STATUS_MAX = 3;

    // Used to mesure the impact of the CanMakePayment return value on whether the Payment Request
    // is shown to the user.
    public static final int CMP_SHOW_COULD_NOT_MAKE_PAYMENT_AND_DID_NOT_SHOW = 0;
    public static final int CMP_SHOW_DID_SHOW = 1 << 0;
    public static final int CMP_SHOW_COULD_MAKE_PAYMENT = 1 << 1;
    public static final int CMP_SHOW_MAX = 4;

    // The events that can occur during a Payment Request.
    public static final int EVENT_INITIATED = 0;
    public static final int EVENT_SHOWN = 1 << 0;
    public static final int EVENT_PAY_CLICKED = 1 << 1;
    public static final int EVENT_RECEIVED_INSTRUMENT_DETAILS = 1 << 2;
    public static final int EVENT_SKIPPED_SHOW = 1 << 3;
    public static final int EVENT_MAX = 16;

    // The minimum expected value of CustomCountHistograms is always set to 1. It is still possible
    // to log the value 0 to that type of histogram.
    private static final int MIN_EXPECTED_SAMPLE = 1;
    private static final int MAX_EXPECTED_SAMPLE = 49;
    private static final int NUMBER_BUCKETS = 50;

    // PaymentRequestAbortReason defined in tools/metrics/histograms/histograms.xml.
    @VisibleForTesting
    public static final int ABORT_REASON_ABORTED_BY_USER = 0;
    @VisibleForTesting
    public static final int ABORT_REASON_ABORTED_BY_MERCHANT = 1;
    @VisibleForTesting
    public static final int ABORT_REASON_INVALID_DATA_FROM_RENDERER = 2;
    @VisibleForTesting
    public static final int ABORT_REASON_MOJO_CONNECTION_ERROR = 3;
    @VisibleForTesting
    public static final int ABORT_REASON_MOJO_RENDERER_CLOSING = 4;
    @VisibleForTesting
    public static final int ABORT_REASON_INSTRUMENT_DETAILS_ERROR = 5;
    @VisibleForTesting
    public static final int ABORT_REASON_NO_MATCHING_PAYMENT_METHOD = 6; // Deprecated.
    @VisibleForTesting
    public static final int ABORT_REASON_NO_SUPPORTED_PAYMENT_METHOD = 7; // Deprecated.
    @VisibleForTesting
    public static final int ABORT_REASON_OTHER = 8;
    @VisibleForTesting
    public static final int ABORT_REASON_MAX = 9;

    // PaymentRequestNoShowReason defined in tools/metrics/histograms/histograms.xml
    @VisibleForTesting
    public static final int NO_SHOW_NO_MATCHING_PAYMENT_METHOD = 0;
    @VisibleForTesting
    public static final int NO_SHOW_NO_SUPPORTED_PAYMENT_METHOD = 1;
    @VisibleForTesting
    public static final int NO_SHOW_CONCURRENT_REQUESTS = 2;
    @VisibleForTesting
    public static final int NO_SHOW_REASON_OTHER = 3;
    @VisibleForTesting
    public static final int NO_SHOW_REASON_MAX = 4;

    /**
     * Pointer to the native implementation.
     */
    private long mJourneyLoggerAndroid;

    private boolean mWasShowCalled;
    private boolean mHasRecorded;

    public JourneyLogger(boolean isIncognito, String url) {
        // Note that this pointer could leak the native object. The called must call destroy() to
        // ensure that the native object is destroyed.
        mJourneyLoggerAndroid = nativeInitJourneyLoggerAndroid(isIncognito, url);
    }

    /** Will destroy the native object. This class shouldn't be used afterwards. */
    public void destroy() {
        if (mJourneyLoggerAndroid != 0) {
            nativeDestroy(mJourneyLoggerAndroid);
            mJourneyLoggerAndroid = 0;
        }
    }

    /**
     * Sets the number of suggestions shown for the specified section.
     *
     * @param section The section for which to log.
     * @param number The number of suggestions.
     */
    public void setNumberOfSuggestionsShown(int section, int number) {
        assert section < SECTION_MAX;
        nativeSetNumberOfSuggestionsShown(mJourneyLoggerAndroid, section, number);
    }

    /**
     * Increments the number of selection changes for the specified section.
     *
     * @param section The section for which to log.
     */
    public void incrementSelectionChanges(int section) {
        assert section < SECTION_MAX;
        nativeIncrementSelectionChanges(mJourneyLoggerAndroid, section);
    }

    /**
     * Increments the number of selection edits for the specified section.
     *
     * @param section The section for which to log.
     */
    public void incrementSelectionEdits(int section) {
        assert section < SECTION_MAX;
        nativeIncrementSelectionEdits(mJourneyLoggerAndroid, section);
    }

    /**
     * Increments the number of selection adds for the specified section.
     *
     * @param section The section for which to log.
     */
    public void incrementSelectionAdds(int section) {
        assert section < SECTION_MAX;
        nativeIncrementSelectionAdds(mJourneyLoggerAndroid, section);
    }

    /**
     * Records the fact that the merchant called CanMakePayment and records it's return value.
     *
     * @param value The return value of the CanMakePayment call.
     */
    public void setCanMakePaymentValue(boolean value) {
        nativeSetCanMakePaymentValue(mJourneyLoggerAndroid, value);
    }

    /**
     * Records the fact that the Payment Request was shown to the user.
     */
    public void setShowCalled() {
        mWasShowCalled = true;
        nativeSetShowCalled(mJourneyLoggerAndroid);
    }

    /**
     * Records that an event occurred.
     */
    public void setEventOccurred(int event) {
        assert event < EVENT_MAX;
        nativeSetEventOccurred(mJourneyLoggerAndroid, event);
    }

    /**
     * Records that the Payment Request was completed sucessfully. Also starts the logging of
     * all the journey logger metrics.
     */
    public void setCompleted() {
        assert !mHasRecorded;
        assert mWasShowCalled;

        if (!mHasRecorded && mWasShowCalled) {
            mHasRecorded = true;
            nativeSetCompleted(mJourneyLoggerAndroid);
        }
    }

    /**
     * Records that the Payment Request was aborted and for what reason. Also starts the logging of
     * all the journey logger metrics.
     *
     * @param reason An int indicating why the payment request was aborted.
     */
    public void setAborted(int reason) {
        assert reason < ABORT_REASON_MAX;
        assert mWasShowCalled;

        // The abort reasons on Android cascade into each other, so only the first one should be
        // recorded.
        if (!mHasRecorded && mWasShowCalled) {
            mHasRecorded = true;
            nativeSetAborted(mJourneyLoggerAndroid, reason);
        }
    }

    /**
     * Records that the Payment Request was not shown to the user and for what reason.
     *
     * @param reason An int indicating why the payment request was not shown.
     */
    public void setNotShown(int reason) {
        assert reason < NO_SHOW_REASON_MAX;
        assert !mWasShowCalled;
        assert !mHasRecorded;

        if (!mHasRecorded) {
            mHasRecorded = true;
            nativeSetNotShown(mJourneyLoggerAndroid, reason);
        }
    }

    private native long nativeInitJourneyLoggerAndroid(boolean isIncognito, String url);
    private native void nativeDestroy(long nativeJourneyLoggerAndroid);
    private native void nativeSetNumberOfSuggestionsShown(
            long nativeJourneyLoggerAndroid, int section, int number);
    private native void nativeIncrementSelectionChanges(
            long nativeJourneyLoggerAndroid, int section);
    private native void nativeIncrementSelectionEdits(long nativeJourneyLoggerAndroid, int section);
    private native void nativeIncrementSelectionAdds(long nativeJourneyLoggerAndroid, int section);
    private native void nativeSetCanMakePaymentValue(
            long nativeJourneyLoggerAndroid, boolean value);
    private native void nativeSetShowCalled(long nativeJourneyLoggerAndroid);
    private native void nativeSetEventOccurred(long nativeJourneyLoggerAndroid, int event);
    private native void nativeSetCompleted(long nativeJourneyLoggerAndroid);
    private native void nativeSetAborted(long nativeJourneyLoggerAndroid, int reason);
    private native void nativeSetNotShown(long nativeJourneyLoggerAndroid, int reason);
}
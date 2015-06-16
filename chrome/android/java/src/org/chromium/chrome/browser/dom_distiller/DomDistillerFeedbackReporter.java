// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.app.Activity;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ChromiumApplication;
import org.chromium.chrome.browser.feedback.FeedbackCollector;
import org.chromium.chrome.browser.feedback.FeedbackReporter;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.WindowAndroid;

/**
 * Java implementation of dom_distiller::android::ExternalFeedbackReporterAndroid.
 */
@JNINamespace("dom_distiller::android")
public final class DomDistillerFeedbackReporter {
    private static final String DISTILLATION_QUALITY_KEY = "Distillation quality";
    private static final String DISTILLATION_QUALITY_GOOD = "good";
    private static final String DISTILLATION_QUALITY_BAD = "bad";

    /**
     * An {@link ExternalFeedbackReporter} that does nothing.
     * TODO(nyquist): Remove when downstream codebase uses {@link FeedbackReporter}.
     */
    @Deprecated
    public static class NoOpExternalFeedbackReporter implements ExternalFeedbackReporter {
        @Override
        public void reportFeedback(Activity activity, String url, boolean good) {
        }
    }

    // TODO(nyquist): Remove when downstream codebase uses {@link FeedbackReporter}.
    private static ExternalFeedbackReporter sExternalFeedbackReporter;

    private static FeedbackReporter sFeedbackReporter;

    /**
     * A static method for native code to call to call the external feedback form.
     * @param window WindowAndroid object to get an activity from.
     * @param url The URL to report feedback for.
     * @param good True if the feedback is good and false if not.
     */
    @CalledByNative
    public static void reportFeedbackWithWindow(WindowAndroid window, String url, boolean good) {
        ThreadUtils.assertOnUiThread();
        Activity activity = window.getActivity().get();
        // TODO(nyquist): Remove when downstream codebase uses {@link FeedbackReporter}.
        if (sExternalFeedbackReporter == null) {
            ChromiumApplication application = (ChromiumApplication) activity.getApplication();
            sExternalFeedbackReporter = application.createDomDistillerFeedbackLauncher();
        }
        // Temporary support old code path.
        if (!(sExternalFeedbackReporter instanceof NoOpExternalFeedbackReporter)) {
            sExternalFeedbackReporter.reportFeedback(activity, url, good);
            return;
        }

        if (sFeedbackReporter == null) {
            ChromiumApplication application = (ChromiumApplication) activity.getApplication();
            sFeedbackReporter = application.createFeedbackReporter();
        }
        FeedbackCollector collector = FeedbackCollector.create(Profile.getLastUsedProfile(), url);
        String quality = good ? DISTILLATION_QUALITY_GOOD : DISTILLATION_QUALITY_BAD;
        collector.add(DISTILLATION_QUALITY_KEY, quality);
        sFeedbackReporter.reportFeedback(activity, collector);
    }

    private DomDistillerFeedbackReporter() {}
}

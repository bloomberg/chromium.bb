// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.child_accounts;

import android.app.Activity;

import org.chromium.base.CalledByNative;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ChromiumApplication;
import org.chromium.chrome.browser.feedback.FeedbackCollector;
import org.chromium.chrome.browser.feedback.FeedbackReporter;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.WindowAndroid;

/**
 * Java implementation of ChildAccountFeedbackReporterAndroid.
 */
public final class ChildAccountFeedbackReporter {
    private static FeedbackReporter sFeedbackReporter;

    public static void reportFeedback(Activity activity,
                                      String description,
                                      String url) {
        ThreadUtils.assertOnUiThread();
        if (sFeedbackReporter == null) {
            ChromiumApplication application = (ChromiumApplication) activity.getApplication();
            sFeedbackReporter = application.createFeedbackReporter();
        }
        FeedbackCollector collector = FeedbackCollector.create(Profile.getLastUsedProfile(), url);
        collector.setDescription(description);
        sFeedbackReporter.reportFeedback(activity, collector);
    }

    @CalledByNative
    public static void reportFeedbackWithWindow(WindowAndroid window,
                                                String description,
                                                String url) {
        reportFeedback(window.getActivity().get(), description, url);
    }

    private ChildAccountFeedbackReporter() {}
}

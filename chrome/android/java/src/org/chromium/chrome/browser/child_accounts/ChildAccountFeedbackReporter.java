// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.child_accounts;

import android.app.Activity;

import org.chromium.base.CalledByNative;
import org.chromium.ui.base.WindowAndroid;

/**
 * Java implementation of ChildAccountFeedbackReporterAndroid.
 */
public final class ChildAccountFeedbackReporter {

    private ChildAccountFeedbackReporter() {}

    private static ExternalFeedbackReporter sExternalFeedbackReporter = null;

    public static void setExternalFeedbackReporter(ExternalFeedbackReporter reporter) {
        sExternalFeedbackReporter = reporter;
    }

    public static void reportFeedback(Activity activity,
                                      String description,
                                      String url) {
        if (sExternalFeedbackReporter != null) {
            sExternalFeedbackReporter.reportFeedback(activity, description, url);
        }
    }

    @CalledByNative
    public static void reportFeedbackWithWindow(WindowAndroid window,
                                                String description,
                                                String url) {
        reportFeedback(window.getActivity().get(), description, url);
    }
}

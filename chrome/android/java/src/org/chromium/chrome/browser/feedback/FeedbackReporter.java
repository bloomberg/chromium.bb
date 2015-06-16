// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.app.Activity;

import javax.annotation.Nullable;

/**
 * FeedbackReporter enables Chrome to send feedback to the feedback server.
 */
public interface FeedbackReporter {
    /**
     * Report feedback to the feedback server.
     *
     * @param activity the activity to take a screenshot of. May be null.
     * @param collector the {@link FeedbackCollector} to use for extra data.
     */
    void reportFeedback(@Nullable Activity activity, FeedbackCollector collector);
}

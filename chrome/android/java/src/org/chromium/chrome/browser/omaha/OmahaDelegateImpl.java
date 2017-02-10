// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.app.Service;
import android.content.Context;
import android.content.pm.ApplicationInfo;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.browser.ChromeApplication;

import java.util.UUID;

/** Delegates calls out from the OmahaClient. */
public class OmahaDelegateImpl extends OmahaDelegate {
    private final ExponentialBackoffScheduler mScheduler;

    OmahaDelegateImpl(Context context) {
        super(context);
        mScheduler = new ExponentialBackoffScheduler(OmahaBase.PREF_PACKAGE, context,
                OmahaClient.MS_POST_BASE_DELAY, OmahaClient.MS_POST_MAX_DELAY);
    }

    @Override
    boolean isInSystemImage() {
        return (getContext().getApplicationInfo().flags & ApplicationInfo.FLAG_SYSTEM) != 0;
    }

    @Override
    ExponentialBackoffScheduler getScheduler() {
        return mScheduler;
    }

    @Override
    String generateUUID() {
        return UUID.randomUUID().toString();
    }

    @Override
    boolean isChromeBeingUsed() {
        boolean isChromeVisible = ApplicationStatus.hasVisibleActivities();
        boolean isScreenOn = ApiCompatibilityUtils.isInteractive(getContext());
        return isChromeVisible && isScreenOn;
    }

    @Override
    void scheduleService(Service service, long nextTimestamp) {
        if (service instanceof OmahaClient) {
            getScheduler().createAlarm(OmahaClient.createOmahaIntent(getContext()), nextTimestamp);
        } else {
            // TODO(dfalcantara): Do something here with a JobService.
        }
    }

    @Override
    protected RequestGenerator createRequestGenerator(Context context) {
        return ((ChromeApplication) context.getApplicationContext()).createOmahaRequestGenerator();
    }
}

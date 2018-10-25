// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;

/**
 * This class updates crash keys when the application state changes.
 */
public class ApplicationStatusTracker {
    private static final String APP_FOREGROUND = "app_foreground";
    private static final String APP_BACKGROUND = "app_background";

    private static class Holder {
        static final ApplicationStatusTracker INSTANCE = new ApplicationStatusTracker();
    }

    private String mCurrentState;

    private ApplicationStatusTracker() {}

    public void registerListener() {
        setApplicationStatus(ApplicationStatus.getStateForApplication());
        ApplicationStatus.registerApplicationStateListener(this::setApplicationStatus);
    }

    private void setApplicationStatus(@ApplicationState int state) {
        ThreadUtils.assertOnUiThread();
        String appStatus;
        // TODO(wnwen): Add foreground service as another state.
        if (isApplicationInForeground(state)) {
            appStatus = APP_FOREGROUND;
        } else {
            appStatus = APP_BACKGROUND;
        }
        if (!appStatus.equals(mCurrentState)) {
            mCurrentState = appStatus;
            CrashKeys.getInstance().set(CrashKeyIndex.APPLICATION_STATUS, appStatus);
        }
    }

    private static boolean isApplicationInForeground(@ApplicationState int state) {
        return state == ApplicationState.HAS_RUNNING_ACTIVITIES
                || state == ApplicationState.HAS_PAUSED_ACTIVITIES;
    }

    /**
     * @return The shared instance of this class.
     */
    public static ApplicationStatusTracker getInstance() {
        return Holder.INSTANCE;
    }
}

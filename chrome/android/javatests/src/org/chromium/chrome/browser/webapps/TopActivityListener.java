// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import javax.annotation.Nullable;

/**
 * Test rule tracking which Chrome activity is currently at the top of the task.
 */
public class TopActivityListener implements TestRule {
    private final ActivityStateListener mListener = new ActivityStateListener() {
        @Override
        public void onActivityStateChange(Activity activity, int newState) {
            if (newState == ActivityState.RESUMED) {
                mMostRecentActivity = activity;
            }
        }
    };

    private Activity mMostRecentActivity;

    @Override
    public Statement apply(final Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                ApplicationStatus.registerStateListenerForAllActivities(mListener);
                base.evaluate();
                ApplicationStatus.unregisterActivityStateListener(mListener);
            }
        };
    }

    @Nullable
    public Activity getMostRecentActivity() {
        return mMostRecentActivity;
    }

    public void waitFor(final Class<? extends Activity> expectedActivityClass) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mMostRecentActivity != null
                        && expectedActivityClass.isAssignableFrom(mMostRecentActivity.getClass());
            }
        });
    }
}

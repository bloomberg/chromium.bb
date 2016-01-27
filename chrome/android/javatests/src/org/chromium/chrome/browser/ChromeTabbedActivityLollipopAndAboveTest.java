// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.document.DocumentActivity;
import org.chromium.chrome.browser.tabmodel.document.AsyncTabCreationParams;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.LoadUrlParams;

import java.lang.ref.WeakReference;
import java.util.List;

/**
 * Tests for ChromeTabbedActivity that need to run on L+ devices.
 */
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
public class ChromeTabbedActivityLollipopAndAboveTest extends ChromeTabbedActivityTestBase {
    /**
     * Confirm that you can't start DocumentActivity while the user is running in tabbed mode.
     */
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @MediumTest
    public void testDontKeepDocumentActivityInTabbedMode() throws Exception {
        // Make sure that ChromeTabbedActivity started up.
        Context context = getInstrumentation().getTargetContext();
        assertFalse(FeatureUtilities.isDocumentMode(context));
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                return lastActivity instanceof ChromeTabbedActivity;
            }
        });

        // Try launching a DocumentActivity.
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                    @Override
                    public void run() {
                        AsyncTabCreationParams asyncParams = new AsyncTabCreationParams(
                                new LoadUrlParams("about:blank"));
                        ChromeLauncherActivity.launchDocumentInstance(null, false, asyncParams);
                    }
                });
            }
        };
        final Activity documentActivity = ActivityUtils.waitForActivity(
                getInstrumentation(), DocumentActivity.class, runnable);

        // ApplicationStatus should note that the DocumentActivity isn't running anymore.
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                List<WeakReference<Activity>> activities = ApplicationStatus.getRunningActivities();
                for (WeakReference<Activity> activity : activities) {
                    if (activity.get() == documentActivity) return false;
                }
                return true;
            }
        });
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}

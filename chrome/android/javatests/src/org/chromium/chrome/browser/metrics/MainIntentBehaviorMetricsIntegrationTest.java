// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.content.Intent;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ContentUrlConstants;

import java.util.concurrent.Callable;

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
public class MainIntentBehaviorMetricsIntegrationTest {
    @Rule
    public ChromeActivityTestRule<ChromeTabbedActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeTabbedActivity.class);

    @MediumTest
    @Test
    public void testFocusOmnibox() throws InterruptedException {
        mActivityTestRule.startMainActivityFromLauncher();
        assertMainIntentBehavior(null);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
                OmniboxTestUtils.toggleUrlBarFocus(urlBar, true);
            }
        });
        assertMainIntentBehavior(MainIntentBehaviorMetrics.FOCUS_OMNIBOX);
    }

    @MediumTest
    @Test
    public void testSwitchTabs() throws InterruptedException {
        mActivityTestRule.startMainActivityFromLauncher();
        assertMainIntentBehavior(null);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mActivityTestRule.getActivity().getTabCreator(false).createNewTab(
                        new LoadUrlParams(ContentUrlConstants.ABOUT_BLANK_URL),
                        TabLaunchType.FROM_RESTORE, null);
            }
        });
        CriteriaHelper.pollUiThread(Criteria.equals(2, new Callable<Integer>() {
            @Override
            public Integer call() throws Exception {
                return mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount();
            }
        }));
        assertMainIntentBehavior(null);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                TabModelUtils.setIndex(mActivityTestRule.getActivity().getCurrentTabModel(), 1);
            }
        });
        assertMainIntentBehavior(MainIntentBehaviorMetrics.SWITCH_TABS);
    }

    @MediumTest
    @Test
    public void testBackgrounded() throws InterruptedException {
        mActivityTestRule.startMainActivityFromLauncher();
        assertMainIntentBehavior(null);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mActivityTestRule.getActivity().finish();
            }
        });
        assertMainIntentBehavior(MainIntentBehaviorMetrics.BACKGROUNDED);
    }

    @MediumTest
    @Test
    public void testCreateNtp() throws InterruptedException {
        mActivityTestRule.startMainActivityFromLauncher();
        assertMainIntentBehavior(null);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mActivityTestRule.getActivity().getTabCreator(false).launchNTP();
            }
        });
        assertMainIntentBehavior(MainIntentBehaviorMetrics.NTP_CREATED);
    }

    @MediumTest
    @Test
    public void testContinuation() throws InterruptedException {
        try {
            MainIntentBehaviorMetrics.setTimeoutDurationMsForTesting(500);
            mActivityTestRule.startMainActivityFromLauncher();
            assertMainIntentBehavior(MainIntentBehaviorMetrics.CONTINUATION);
        } finally {
            MainIntentBehaviorMetrics.setTimeoutDurationMsForTesting(
                    MainIntentBehaviorMetrics.TIMEOUT_DURATION_MS);
        }
    }

    @MediumTest
    @Test
    public void testMainIntentWithoutLauncherCategory() throws InterruptedException {
        mActivityTestRule.startMainActivityFromIntent(new Intent(Intent.ACTION_MAIN), null);
        assertMainIntentBehavior(null);
        Assert.assertFalse(mActivityTestRule.getActivity().getMainIntentBehaviorMetricsForTesting()
                .getPendingActionRecordForMainIntent());
    }

    private void assertMainIntentBehavior(Integer expected) {
        CriteriaHelper.pollUiThread(Criteria.equals(expected, new Callable<Integer>() {
            @Override
            public Integer call() throws Exception {
                return mActivityTestRule.getActivity()
                        .getMainIntentBehaviorMetricsForTesting()
                        .getLastMainIntentBehaviorForTesting();
            }
        }));
    }
}

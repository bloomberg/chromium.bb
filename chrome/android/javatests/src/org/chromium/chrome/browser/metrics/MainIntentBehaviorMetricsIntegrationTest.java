// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.content.ComponentName;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;
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
    public void testFocusOmnibox() {
        startActivity(true);
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
    public void testSwitchTabs() {
        startActivity(true);
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
    public void testBackgrounded() {
        startActivity(true);
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
    public void testCreateNtp() {
        startActivity(true);
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
    public void testContinuation() {
        try {
            MainIntentBehaviorMetrics.setTimeoutDurationMsForTesting(500);
            startActivity(true);
            assertMainIntentBehavior(MainIntentBehaviorMetrics.CONTINUATION);
        } finally {
            MainIntentBehaviorMetrics.setTimeoutDurationMsForTesting(
                    MainIntentBehaviorMetrics.TIMEOUT_DURATION_MS);
        }
    }

    @MediumTest
    @Test
    public void testMainIntentWithoutLauncherCategory() {
        startActivity(false);
        assertMainIntentBehavior(null);
        Assert.assertFalse(mActivityTestRule.getActivity().getMainIntentBehaviorMetricsForTesting()
                .getPendingActionRecordForMainIntent());
    }

    private void startActivity(boolean addLauncherCategory) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        if (addLauncherCategory) intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.setComponent(new ComponentName(
                InstrumentationRegistry.getTargetContext(), ChromeTabbedActivity.class));

        mActivityTestRule.startActivityCompletely(intent);
        mActivityTestRule.waitForActivityNativeInitializationComplete();
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

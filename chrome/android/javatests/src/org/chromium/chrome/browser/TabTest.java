// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * Tests for Tab class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
public class TabTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private Tab mTab;
    private CallbackHelper mOnTitleUpdatedHelper;

    private final TabObserver mTabObserver = new EmptyTabObserver() {
        @Override
        public void onTitleUpdated(Tab tab) {
            mOnTitleUpdatedHelper.notifyCalled();
        }
    };

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTab = mActivityTestRule.getActivity().getActivityTab();
        mTab.addObserver(mTabObserver);
        mOnTitleUpdatedHelper = new CallbackHelper();
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testTabContext() throws Throwable {
        Assert.assertFalse("The tab context cannot be an activity",
                mTab.getContentViewCore().getContext() instanceof Activity);
        Assert.assertNotSame("The tab context's theme should have been updated",
                mTab.getContentViewCore().getContext().getTheme(),
                mActivityTestRule.getActivity().getApplication().getTheme());
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testTitleDelayUpdate() throws Throwable {
        final String oldTitle = "oldTitle";
        final String newTitle = "newTitle";

        mActivityTestRule.loadUrl("data:text/html;charset=utf-8,<html><head><title>" + oldTitle
                + "</title></head><body/></html>");
        Assert.assertEquals("title does not match initial title", oldTitle, mTab.getTitle());
        int currentCallCount = mOnTitleUpdatedHelper.getCallCount();
        mActivityTestRule.runJavaScriptCodeInCurrentTab("document.title='" + newTitle + "';");
        mOnTitleUpdatedHelper.waitForCallback(currentCallCount);
        Assert.assertEquals("title does not update", newTitle, mTab.getTitle());
    }

    /**
     * Verifies a Tab's contents is restored when the Tab is foregrounded
     * after its contents have been destroyed while backgrounded.
     * Note that document mode is explicitly disabled, as the document activity
     * may be fully recreated if its contents is killed while in the background.
     */
    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testTabRestoredIfKilledWhileActivityStopped() throws Exception {
        // Ensure the tab is showing before stopping the activity.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mTab.show(TabSelectionType.FROM_NEW);
            }
        });

        Assert.assertFalse(mTab.needsReload());
        Assert.assertFalse(mTab.isHidden());
        Assert.assertFalse(mTab.isShowingSadTab());

        // Stop the activity and simulate a killed renderer.
        ApplicationTestUtils.fireHomeScreenIntent(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mTab.simulateRendererKilledForTesting(false);
            }
        });

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mTab.isHidden();
            }
        });
        Assert.assertTrue(mTab.needsReload());
        Assert.assertFalse(mTab.isShowingSadTab());

        ApplicationTestUtils.launchChrome(
                InstrumentationRegistry.getInstrumentation().getTargetContext());

        // The tab should be restored and visible.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !mTab.isHidden();
            }
        });
        Assert.assertFalse(mTab.needsReload());
        Assert.assertFalse(mTab.isShowingSadTab());
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testTabSecurityLevel() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertEquals(ConnectionSecurityLevel.NONE, mTab.getSecurityLevel());
            }
        });
    }
}

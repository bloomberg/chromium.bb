// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.content.Intent;
import android.provider.Browser;
import android.support.test.filters.MediumTest;
import android.support.test.rule.UiThreadTestRule;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.media.MediaSwitches;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/**
 * Tests for FullscreenActivity.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        MediaSwitches.IGNORE_AUTOPLAY_RESTRICTIONS_FOR_TESTS,
        "enable-features=" + ChromeFeatureList.FULLSCREEN_ACTIVITY})
public class FullscreenActivityTest {
    private static final String TEST_PATH = "/content/test/data/media/video-player.html";
    private static final String VIDEO_ID = "video";

    @Rule
    public UiThreadTestRule mUiThreadTestRule = new UiThreadTestRule();
    @Rule
    public ChromeActivityTestRule<ChromeTabbedActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeTabbedActivity.class);

    private EmbeddedTestServer mTestServer;
    private ChromeTabbedActivity mActivity;

    @Before
    public void setUp() throws InterruptedException {
        mTestServer = EmbeddedTestServer.createAndStartServer(
                mActivityTestRule.getInstrumentation().getContext());
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(TEST_PATH));
        mActivity = mActivityTestRule.getActivity();
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    /**
     * Manually moves a Tab from a ChromeTabbedActivity to a FullscreenActivity and back.
     */
    @Test
    @MediumTest
    public void testTransfer() throws Throwable {
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        moveTabToActivity(mActivity, tab, FullscreenActivity.class);
        waitForActivity(FullscreenActivity.class);

        final FullscreenActivity fullscreenActivity =
                (FullscreenActivity) ApplicationStatus.getLastTrackedFocusedActivity();
        Assert.assertEquals(tab.getId(), fullscreenActivity.getActivityTab().getId());

        moveTabToActivity(fullscreenActivity, tab, ChromeTabbedActivity.class);
        waitForActivity(ChromeTabbedActivity.class);
    }

    /**
     * Manually moves the Tab to an Activity.
     */
    private void moveTabToActivity(final Activity fromActivity, final Tab tab,
            final Class<? extends ChromeActivity> targetClass) throws Throwable {
        mUiThreadTestRule.runOnUiThread(() -> {
            Intent intent = new Intent(fromActivity, targetClass);
            intent.putExtra(
                    IntentHandler.EXTRA_PARENT_COMPONENT, fromActivity.getComponentName());
            intent.putExtra(Browser.EXTRA_APPLICATION_ID, fromActivity.getPackageName());

            if (targetClass == FullscreenActivity.class) {
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            }

            tab.detachAndStartReparenting(intent, null, null);
        });
    }

    /**
     * Waits for an Activity of the given class to be started and for it to have a Tab.
     * @return The Activity.
     */
    @SuppressWarnings("unchecked")
    private static <T extends ChromeActivity> T waitForActivity(final Class<T> activityClass) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                if (lastActivity.getClass() != activityClass) return false;

                return ((ChromeActivity) lastActivity).getActivityTab() != null;
            }
        });
        return (T) ApplicationStatus.getLastTrackedFocusedActivity();
    }

    /**
     * Toggles fullscreen to check FullscreenActivity has been started.
     */
    @Test
    @MediumTest
    public void testFullscreen() throws Throwable {
        Activity original = mActivity;

        FullscreenActivity fullscreenActivity = enterFullscreen();
        DOMUtils.exitFullscreen(fullscreenActivity.getCurrentContentViewCore().getWebContents());

        ChromeTabbedActivity activity = waitForActivity(ChromeTabbedActivity.class);

        // Ensure we haven't started a new ChromeTabbedActivity, https://crbug.com/729805,
        // https://crbug.com/729932.
        Assert.assertSame(original, activity);
    }

    /**
     * Enters fullscreen then presses the back button to exit.
     */
    @Test
    @MediumTest
    public void testExitOnBack() throws Throwable {
        Activity original = mActivity;
        final FullscreenActivity fullscreenActivity = enterFullscreen();
        mUiThreadTestRule.runOnUiThread(() -> fullscreenActivity.onBackPressed());

        ChromeTabbedActivity activity = waitForActivity(ChromeTabbedActivity.class);

        // Ensure we haven't started a new ChromeTabbedActivity, https://crbug.com/729805,
        // https://crbug.com/729932.
        Assert.assertSame(original, activity);
    }

    /**
     * Clicks on the fullscreen button in the test page, waits for the FullscreenActivity
     * to be started and for it to go fullscreen.
     */
    private FullscreenActivity enterFullscreen() throws Throwable {
        // Start playback to guarantee it's properly loaded.
        WebContents webContents = mActivity.getCurrentContentViewCore().getWebContents();
        Assert.assertTrue(DOMUtils.isMediaPaused(webContents, VIDEO_ID));
        DOMUtils.playMedia(webContents, VIDEO_ID);
        DOMUtils.waitForMediaPlay(webContents, VIDEO_ID);

        // Trigger requestFullscreen() via a click on a button.
        Assert.assertTrue(DOMUtils.clickNode(mActivity.getCurrentContentViewCore(), "fullscreen"));

        final FullscreenActivity fullscreenActivity = waitForActivity(FullscreenActivity.class);

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return DOMUtils.isFullscreen(
                            fullscreenActivity.getCurrentContentViewCore().getWebContents());
                } catch (InterruptedException | TimeoutException e) {
                    throw new RuntimeException(e);
                }
            }
        });

        return fullscreenActivity;
    }

    /**
     * Tests that no flags change on the ChromeTabbedActivity when going fullscreen.
     */
    @Test
    @MediumTest
    public void testFullscreenFlags() throws Throwable {
        int old = mActivity.getTabsView().getSystemUiVisibility();

        FullscreenActivity fullscreenActivity = enterFullscreen();
        DOMUtils.exitFullscreen(fullscreenActivity.getCurrentContentViewCore().getWebContents());

        waitForActivity(ChromeTabbedActivity.class);

        Assert.assertEquals(old, mActivity.getTabsView().getSystemUiVisibility());
    }
}

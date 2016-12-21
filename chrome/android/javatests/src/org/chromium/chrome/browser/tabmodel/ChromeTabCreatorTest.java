// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_LOW_END_DEVICE;
import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.support.test.filters.MediumTest;
import android.test.UiThreadTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Tests for ChromeTabCreator.
 */
public class ChromeTabCreatorTest extends ChromeTabbedActivityTestBase  {
    private static final String TEST_PATH = "/chrome/test/data/android/about.html";

    private EmbeddedTestServer mTestServer;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    /**
     * Verify that tabs opened in background on low-end are loaded lazily.
     */
    @Restriction(RESTRICTION_TYPE_LOW_END_DEVICE)
    @MediumTest
    @Feature({"Browser"})
    public void testCreateNewTabInBackgroundLowEnd()
            throws ExecutionException, InterruptedException {
        final Tab fgTab = getActivity().getActivityTab();
        final Tab bgTab = ThreadUtils.runOnUiThreadBlocking(new Callable<Tab>() {
            @Override
            public Tab call() {
                return getActivity().getCurrentTabCreator().createNewTab(
                        new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND, fgTab);
            }
        });

        // Verify that the background tab is not loading.
        assertFalse(bgTab.isLoading());

        // Switch tabs and verify that the tab is loaded as it gets foregrounded.
        ChromeTabUtils.waitForTabPageLoaded(bgTab, new Runnable() {
            @Override
            public void run() {
                ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                    @Override
                    public void run() {
                        TabModelUtils.setIndex(getActivity().getCurrentTabModel(), indexOf(bgTab));
                    }
                });
            }
        });
        assertNotNull(bgTab.getView());
    }

    /**
     * Verify that tabs opened in background on regular devices are loaded eagerly.
     */
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @MediumTest
    @Feature({"Browser"})
    public void testCreateNewTabInBackground()
            throws ExecutionException, InterruptedException {
        final Tab fgTab = getActivity().getActivityTab();
        Tab bgTab = ThreadUtils.runOnUiThreadBlocking(new Callable<Tab>() {
            @Override
            public Tab call() {
                return getActivity().getCurrentTabCreator().createNewTab(
                        new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND, fgTab);
            }
        });

        // Verify that the background tab is loaded.
        assertNotNull(bgTab.getView());
        ChromeTabUtils.waitForTabPageLoaded(bgTab, mTestServer.getURL(TEST_PATH));
    }

    /**
     * Verify that the spare WebContents is used.
     *
     * Spare WebContents are not created on low-devices, so don't run the test.
     */
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @MediumTest
    @Feature({"Browser"})
    @UiThreadTest
    public void testCreateNewTabTakesSpareWebContents()
            throws ExecutionException, InterruptedException {
        Tab currentTab = getActivity().getActivityTab();
        WarmupManager.getInstance().createSpareWebContents();
        assertTrue(WarmupManager.getInstance().hasSpareWebContents());
        getActivity().getCurrentTabCreator().createNewTab(
                new LoadUrlParams(mTestServer.getURL(TEST_PATH)), TabLaunchType.FROM_EXTERNAL_APP,
                currentTab);
        assertFalse(WarmupManager.getInstance().hasSpareWebContents());
    }

    /**
     * @return the index of the given tab in the current tab model
     */
    private int indexOf(Tab tab) {
        return getActivity().getCurrentTabModel().indexOf(tab);
    }


    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}

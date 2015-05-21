// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_LOW_END_DEVICE;
import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.tab.ChromeTab;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Tests for ChromeTabCreator.
 */
public class ChromeTabCreatorTest extends ChromeTabbedActivityTestBase  {
    private static final String TEST_URL =
            TestHttpServerClient.getUrl("chrome/test/data/android/about.html");

    /**
     * Verify that tabs opened in background on low-end are loaded lazily.
     */
    @Restriction(RESTRICTION_TYPE_LOW_END_DEVICE)
    @MediumTest
    @Feature({"Browser"})
    public void testCreateNewTabInBackgroundLowEnd()
            throws ExecutionException, InterruptedException {
        final ChromeTab fgTab = ChromeTab.fromTab(getActivity().getActivityTab());
        final ChromeTab bgTab = ThreadUtils.runOnUiThreadBlocking(new Callable<ChromeTab>() {
            @Override
            public ChromeTab call() {
                return getActivity().getCurrentTabCreator().createNewTab(
                        new LoadUrlParams(TEST_URL), TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                        fgTab);
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
        final ChromeTab fgTab = ChromeTab.fromTab(getActivity().getActivityTab());
        ChromeTab bgTab = ThreadUtils.runOnUiThreadBlocking(new Callable<ChromeTab>() {
            @Override
            public ChromeTab call() {
                return getActivity().getCurrentTabCreator().createNewTab(
                        new LoadUrlParams(TEST_URL), TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                        fgTab);
            }
        });

        // Verify that the background tab is loaded.
        assertNotNull(bgTab.getView());
        ChromeTabUtils.waitForTabPageLoaded(bgTab, TEST_URL);
    }

    /**
     * @return the index of the given tab in the current tab model
     */
    private int indexOf(ChromeTab tab) {
        return getActivity().getCurrentTabModel().indexOf(tab);
    }


    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}

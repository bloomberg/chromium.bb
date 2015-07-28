// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offline_pages;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.offline_pages.OfflinePageBridge.OfflinePageCallback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.components.offline_pages.LoadResult;

import java.util.List;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link OfflinePageBridge}. */
public class OfflinePageBridgeTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String TEST_PAGE =
            TestHttpServerClient.getUrl("chrome/test/data/android/about.html");
    private static final int TIMEOUT_MS = 5000;

    public OfflinePageBridgeTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @MediumTest
    public void testLoadOfflinePagesWhenEmpty() throws Exception {
        loadAllPages(LoadResult.SUCCESS, /* expected count */ 0);
    }

    private void loadAllPages(final int expectedResult, final int expectedCount)
            throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Profile profile = Profile.getLastUsedProfile();
                final OfflinePageBridge offlinePageBridge = new OfflinePageBridge(profile);
                offlinePageBridge.loadAllPages(new OfflinePageCallback() {
                    @Override
                    public void onLoadAllPagesDone(
                            int loadResult, List<OfflinePageItem> offlinePages) {
                        assertEquals("Save result incorrect.", expectedResult, loadResult);
                        assertEquals("Offline pages count incorrect.",
                                expectedCount, offlinePages.size());
                        semaphore.release();
                    }
                });
            }
        });
        assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }
}

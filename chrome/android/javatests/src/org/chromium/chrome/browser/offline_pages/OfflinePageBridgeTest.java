// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offline_pages;

import android.test.FlakyTest;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.offline_pages.OfflinePageBridge.OfflinePageCallback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.offline_pages.LoadResult;
import org.chromium.components.offline_pages.SavePageResult;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link OfflinePageBridge}. */
public class OfflinePageBridgeTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String TEST_PAGE =
            TestHttpServerClient.getUrl("chrome/test/data/android/about.html");
    private static final int TIMEOUT_MS = 5000;
    private static final BookmarkId BOOKMARK_ID = new BookmarkId(1234, BookmarkType.NORMAL);

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

    /*
    @MediumTest
    Bug 518758
    */
    @FlakyTest
    public void testAddOfflinePageAndLoad() throws Exception {
        loadUrl(TEST_PAGE);
        savePage(SavePageResult.SUCCESS, TEST_PAGE);
        List<OfflinePageItem> allPages = loadAllPages(0 /* LoadResult.SUCCESS */, 1);
        assertEquals("Offline page item url incorrect.", TEST_PAGE, allPages.get(0).getUrl());
        assertEquals("Offline page item bookmark ID incorrect.", BOOKMARK_ID,
                allPages.get(0).getBookmarkId());
        assertTrue("Offline page item offline file url doesn't start properly.",
                allPages.get(0).getOfflineUrl().startsWith("file:///"));
        assertTrue("Offline page item offline file doesn't have the right name.",
                allPages.get(0).getOfflineUrl().endsWith("About.mhtml"));
        assertEquals("Offline page item size incorrect.", 626, allPages.get(0).getFileSize());
    }

    private void savePage(final int expectedResult, final String expectedUrl)
            throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertNotNull("Tab is null", getActivity().getActivityTab());
                assertEquals("URL does not match requested.", TEST_PAGE,
                        getActivity().getActivityTab().getUrl());
                assertNotNull("WebContents is null",
                        getActivity().getActivityTab().getWebContents());

                Profile profile = Profile.getLastUsedProfile();
                final OfflinePageBridge offlinePageBridge = new OfflinePageBridge(profile);
                offlinePageBridge.savePage(getActivity().getActivityTab().getWebContents(),
                        BOOKMARK_ID, new OfflinePageCallback() {
                            @Override
                            public void onLoadAllPagesDone(
                                    int loadResult, List<OfflinePageItem> offlinePages) {
                                assertTrue("Should have received this callback", false);
                            }

                            @Override
                            public void onSavePageDone(int savePageResult, String url) {
                                assertEquals(
                                        "Requested and returned URLs differ.", expectedUrl, url);
                                assertEquals(
                                        "Save result incorrect.", expectedResult, savePageResult);
                                semaphore.release();
                            }
                        });
            }
        });
        assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    private List<OfflinePageItem> loadAllPages(final int expectedResult, final int expectedCount)
            throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        final List<OfflinePageItem> result = new ArrayList<OfflinePageItem>();
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

                        // Ensuring the result collection has a proper size before copying.
                        for (int i = 0; i < offlinePages.size(); i++) {
                            result.add(null);
                        }
                        Collections.copy(result, offlinePages);
                        semaphore.release();
                    }

                    @Override
                    public void onSavePageDone(int savePageResult, String url) {
                        assertTrue("Should have received this callback", false);
                    }
                });
            }
        });
        assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        return result;
    }
}

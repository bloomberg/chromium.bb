// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.DeletePageCallback;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.OfflinePageModelObserver;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.SavePageCallback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.offlinepages.DeletePageResult;
import org.chromium.components.offlinepages.SavePageResult;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link OfflinePageBridge}. */
public class OfflinePageBridgeTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String TEST_PAGE =
            TestHttpServerClient.getUrl("chrome/test/data/android/about.html");
    private static final int TIMEOUT_MS = 5000;
    private static final BookmarkId BOOKMARK_ID = new BookmarkId(1234, BookmarkType.NORMAL);

    private OfflinePageBridge mOfflinePageBridge;

    public OfflinePageBridgeTest() {
        super(ChromeActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Profile profile = Profile.getLastUsedProfile();
                mOfflinePageBridge = new OfflinePageBridge(profile);
                if (mOfflinePageBridge.isOfflinePageModelLoaded()) {
                    semaphore.release();
                } else {
                    mOfflinePageBridge.addObserver(new OfflinePageModelObserver() {
                        @Override
                        public void offlinePageModelLoaded() {
                            semaphore.release();
                            mOfflinePageBridge.removeObserver(this);
                        }
                    });
                }
            }
        });
        assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @MediumTest
    public void testLoadOfflinePagesWhenEmpty() throws Exception {
        List<OfflinePageItem> offlinePages = getAllPages();
        assertEquals("Offline pages count incorrect.", 0, offlinePages.size());
    }

    @MediumTest
    public void testAddOfflinePageAndLoad() throws Exception {
        loadUrl(TEST_PAGE);
        savePage(SavePageResult.SUCCESS, TEST_PAGE);
        List<OfflinePageItem> allPages = getAllPages();
        OfflinePageItem offlinePage = allPages.get(0);
        assertEquals("Offline pages count incorrect.", 1, allPages.size());
        assertEquals("Offline page item url incorrect.", TEST_PAGE, offlinePage.getUrl());
        assertEquals("Offline page item bookmark ID incorrect.", BOOKMARK_ID,
                offlinePage.getBookmarkId());
        assertTrue("Offline page item offline file url doesn't start properly.",
                offlinePage.getOfflineUrl().startsWith("file:///"));
        assertTrue("Offline page item offline file doesn't have the right name.",
                offlinePage.getOfflineUrl().endsWith(".mhtml"));
        assertTrue("Offline page item offline file doesn't have the right name.",
                offlinePage.getOfflineUrl().contains("About"));
        // BUG(518758): Depending on the bot the result will be either 626 or 627.
        long size = offlinePage.getFileSize();
        assertTrue("Offline page item size is incorrect: " + size, size == 626 || size == 627);
    }

    @MediumTest
    public void testMarkPageAccessed() throws Exception {
        loadUrl(TEST_PAGE);
        savePage(SavePageResult.SUCCESS, TEST_PAGE);
        OfflinePageItem offlinePage = mOfflinePageBridge.getPageByBookmarkId(BOOKMARK_ID);
        assertNotNull("Offline page should be available, but it is not.", offlinePage);
        assertEquals("Offline page access count should be 0.", 0, offlinePage.getAccessCount());

        markPageAccessed(BOOKMARK_ID, 1);
    }

    @MediumTest
    public void testGetPageByBookmarkId() throws Exception {
        loadUrl(TEST_PAGE);
        savePage(SavePageResult.SUCCESS, TEST_PAGE);
        OfflinePageItem offlinePage = mOfflinePageBridge.getPageByBookmarkId(BOOKMARK_ID);
        assertEquals("Offline page item url incorrect.", TEST_PAGE, offlinePage.getUrl());
        assertEquals("Offline page item bookmark ID incorrect.", BOOKMARK_ID,
                offlinePage.getBookmarkId());
        assertTrue("Offline page item offline file url doesn't start properly.",
                offlinePage.getOfflineUrl().startsWith("file:///"));
        assertTrue("Offline page item offline file doesn't have the right name.",
                offlinePage.getOfflineUrl().endsWith(".mhtml"));
        assertTrue("Offline page item offline file doesn't have the right name.",
                offlinePage.getOfflineUrl().contains("About"));

        assertNull("Offline page is not supposed to exist",
                mOfflinePageBridge.getPageByBookmarkId(new BookmarkId(-42, BookmarkType.NORMAL)));
    }

    @MediumTest
    public void testDeleteOfflinePage() throws Exception {
        deletePage(BOOKMARK_ID, DeletePageResult.NOT_FOUND);
        loadUrl(TEST_PAGE);
        savePage(SavePageResult.SUCCESS, TEST_PAGE);
        assertNotNull("Offline page should be available, but it is not.",
                mOfflinePageBridge.getPageByBookmarkId(BOOKMARK_ID));
        deletePage(BOOKMARK_ID, DeletePageResult.SUCCESS);
        assertNull("Offline page should be gone, but it is available.",
                mOfflinePageBridge.getPageByBookmarkId(BOOKMARK_ID));
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

                mOfflinePageBridge.savePage(getActivity().getActivityTab().getWebContents(),
                        BOOKMARK_ID, new SavePageCallback() {
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

    private void markPageAccessed(final BookmarkId bookmarkId, final int expectedAccessCount)
            throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mOfflinePageBridge.markPageAccessed(bookmarkId);
            }
        });
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                OfflinePageItem offlinePage =
                        mOfflinePageBridge.getPageByBookmarkId(bookmarkId);
                return offlinePage.getAccessCount() == expectedAccessCount;
            }
        }));
    }

    private void deletePage(BookmarkId bookmarkId, final int expectedResult)
            throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mOfflinePageBridge.deletePage(BOOKMARK_ID, new DeletePageCallback() {
                    @Override
                    public void onDeletePageDone(int deletePageResult) {
                        assertEquals("Delete result incorrect.", expectedResult, deletePageResult);
                        semaphore.release();
                    }
                });
            }
        });
        assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    private List<OfflinePageItem> getAllPages()
            throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        final List<OfflinePageItem> result = new ArrayList<OfflinePageItem>();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                result.clear();
                for (OfflinePageItem item : mOfflinePageBridge.getAllPages()) {
                    result.add(item);
                }

                semaphore.release();
            }
        });
        assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        return result;
    }
}

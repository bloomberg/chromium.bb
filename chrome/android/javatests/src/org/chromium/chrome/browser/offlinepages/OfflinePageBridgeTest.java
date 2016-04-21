// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.content.Context;
import android.os.Environment;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.DeletePageCallback;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.MultipleOfflinePageItemCallback;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.OfflinePageModelObserver;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.SavePageCallback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.components.offlinepages.DeletePageResult;
import org.chromium.components.offlinepages.SavePageResult;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

/** Unit tests for {@link OfflinePageBridge}. */
@CommandLineFlags.Add("enable-features=OfflineBookmarks")
public class OfflinePageBridgeTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String TEST_PAGE = "/chrome/test/data/android/about.html";
    private static final int TIMEOUT_MS = 5000;
    private static final long POLLING_INTERVAL = 100;
    private static final ClientId BOOKMARK_ID =
            new ClientId(OfflinePageBridge.BOOKMARK_NAMESPACE, "1234");

    private OfflinePageBridge mOfflinePageBridge;
    private EmbeddedTestServer mTestServer;
    private String mTestPage;

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
                // Ensure we start in an offline state.
                NetworkChangeNotifier.forceConnectivityState(false);
                Context context = getActivity().getBaseContext();
                if (!NetworkChangeNotifier.isInitialized()) {
                    NetworkChangeNotifier.init(context);
                }
                Profile profile = Profile.getLastUsedProfile();
                mOfflinePageBridge = OfflinePageBridge.getForProfile(profile);
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

        mTestServer = EmbeddedTestServer.createAndStartFileServer(
                getInstrumentation().getContext(), Environment.getExternalStorageDirectory());
        mTestPage = mTestServer.getURL(TEST_PAGE);
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @SmallTest
    public void testLoadOfflinePagesWhenEmpty() throws Exception {
        List<OfflinePageItem> offlinePages = getAllPages();
        assertEquals("Offline pages count incorrect.", 0, offlinePages.size());
    }

    @SmallTest
    public void testAddOfflinePageAndLoad() throws Exception {
        loadUrl(mTestPage);
        savePage(SavePageResult.SUCCESS, mTestPage);
        List<OfflinePageItem> allPages = getAllPages();
        OfflinePageItem offlinePage = allPages.get(0);
        assertEquals("Offline pages count incorrect.", 1, allPages.size());
        assertEquals("Offline page item url incorrect.", mTestPage, offlinePage.getUrl());
        assertTrue("Offline page item offline file url doesn't start properly.",
                offlinePage.getOfflineUrl().startsWith("file:///"));
        assertTrue("Offline page item offline file doesn't have the right name.",
                offlinePage.getOfflineUrl().endsWith(".mhtml"));
        assertTrue("Offline page item offline file doesn't have the right name.",
                offlinePage.getOfflineUrl().contains("About"));

        // We don't care about the exact file size of the mhtml file:
        // - exact file size is not something that the end user sees or cares about
        // - exact file size can vary based on external factors (i.e. see crbug.com/518758)
        // - verification of contents of the resulting mhtml file should be covered by mhtml
        //   serialization tests (i.e. save_page_browsertest.cc)
        // - we want to avoid overtesting and artificially requiring specific formatting and/or
        //   implementation choices in the mhtml serialization code
        // OTOH, it still seems useful to assert that the file is not empty and that its size is in
        // the right ballpark.
        long size = offlinePage.getFileSize();
        assertTrue("Offline page item size is incorrect: " + size, 600 < size && size < 800);
    }

    @SmallTest
    public void testGetPageByBookmarkId() throws Exception {
        loadUrl(mTestPage);
        savePage(SavePageResult.SUCCESS, mTestPage);
        OfflinePageItem offlinePage = getPageByClientId(BOOKMARK_ID);
        assertEquals("Offline page item url incorrect.", mTestPage, offlinePage.getUrl());
        assertTrue("Offline page item offline file url doesn't start properly.",
                offlinePage.getOfflineUrl().startsWith("file:///"));
        assertTrue("Offline page item offline file doesn't have the right name.",
                offlinePage.getOfflineUrl().endsWith(".mhtml"));
        assertTrue("Offline page item offline file doesn't have the right name.",
                offlinePage.getOfflineUrl().contains("About"));

        assertNull("Offline page is not supposed to exist",
                getPageByClientId(new ClientId(OfflinePageBridge.BOOKMARK_NAMESPACE, "-42")));
    }

    @SmallTest
    public void testDeleteOfflinePage() throws Exception {
        deletePage(BOOKMARK_ID, DeletePageResult.NOT_FOUND);
        loadUrl(mTestPage);
        savePage(SavePageResult.SUCCESS, mTestPage);
        assertNotNull(
                "Offline page should be available, but it is not.", getPageByClientId(BOOKMARK_ID));
        deletePage(BOOKMARK_ID, DeletePageResult.SUCCESS);
        assertNull("Offline page should be gone, but it is available.",
                getPageByClientId(BOOKMARK_ID));
    }

    @SmallTest
    public void testGetOfflineUrlForOnlineUrl() throws Exception {
        loadUrl(mTestPage);
        savePage(SavePageResult.SUCCESS, mTestPage);
        OfflinePageItem offlinePage = getPageByClientId(BOOKMARK_ID);
        assertEquals("We should get the same offline URL, when querying using online URL",
                offlinePage.getOfflineUrl(),
                mOfflinePageBridge.getOfflineUrlForOnlineUrl(offlinePage.getUrl()));
    }

    @CommandLineFlags.Add("disable-features=OfflinePagesBackgroundLoading")
    @SmallTest
    public void testBackgroundLoadSwitch() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertFalse("If background loading is off, we should see the feature disabled",
                        OfflinePageBridge.isBackgroundLoadingEnabled());
            }
        });
    }

    private void savePage(final int expectedResult, final String expectedUrl)
            throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertNotNull("Tab is null", getActivity().getActivityTab());
                assertEquals("URL does not match requested.", mTestPage,
                        getActivity().getActivityTab().getUrl());
                assertNotNull("WebContents is null",
                        getActivity().getActivityTab().getWebContents());

                mOfflinePageBridge.savePage(getActivity().getActivityTab().getWebContents(),
                        BOOKMARK_ID, new SavePageCallback() {
                            @Override
                            public void onSavePageDone(
                                    int savePageResult, String url, long offlineId) {
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

    private void deletePage(final ClientId bookmarkId, final int expectedResult)
            throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        final AtomicInteger deletePageResultRef = new AtomicInteger();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mOfflinePageBridge.deletePage(bookmarkId, new DeletePageCallback() {
                    @Override
                    public void onDeletePageDone(int deletePageResult) {
                        deletePageResultRef.set(deletePageResult);
                        semaphore.release();
                    }
                });
            }
        });
        assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        assertEquals("Delete result incorrect.", expectedResult, deletePageResultRef.get());
    }

    private List<OfflinePageItem> getAllPages() throws InterruptedException {
        final List<OfflinePageItem> result = new ArrayList<OfflinePageItem>();
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mOfflinePageBridge.getAllPages(new MultipleOfflinePageItemCallback() {
                    @Override
                    public void onResult(List<OfflinePageItem> pages) {
                        result.addAll(pages);
                        semaphore.release();
                    }
                });
            }
        });
        assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        return result;
    }

    private void forceConnectivityStateOnUiThread(final boolean state) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                NetworkChangeNotifier.forceConnectivityState(state);
            }
        });
    }

    private OfflinePageItem getPageByClientId(final ClientId clientId) throws InterruptedException {
        final OfflinePageItem[] result = {null};
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mOfflinePageBridge.getPagesByClientId(
                        clientId, new MultipleOfflinePageItemCallback() {
                            @Override
                            public void onResult(List<OfflinePageItem> items) {
                                if (!items.isEmpty()) {
                                    result[0] = items.get(0);
                                }
                                semaphore.release();
                            }
                        });
            }
        });
        assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        return result[0];
    }
}

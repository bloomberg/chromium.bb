// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.OfflinePageModelObserver;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.SavePageCallback;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.offlinepages.DeletePageResult;
import org.chromium.components.offlinepages.SavePageResult;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for {@link OfflinePageBridge}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
public class OfflinePageBridgeTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String TEST_PAGE = "/chrome/test/data/android/about.html";
    private static final int TIMEOUT_MS = 5000;
    private static final long POLLING_INTERVAL = 100;
    private static final ClientId BOOKMARK_ID =
            new ClientId(OfflinePageBridge.BOOKMARK_NAMESPACE, "1234");

    private OfflinePageBridge mOfflinePageBridge;
    private EmbeddedTestServer mTestServer;
    private String mTestPage;

    private void initializeBridgeForProfile(final boolean incognitoProfile)
            throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Profile profile = Profile.getLastUsedProfile();
                if (incognitoProfile) {
                    profile = profile.getOffTheRecordProfile();
                }
                // Ensure we start in an offline state.
                mOfflinePageBridge = OfflinePageBridge.getForProfile(profile);
                if (mOfflinePageBridge == null || mOfflinePageBridge.isOfflinePageModelLoaded()) {
                    semaphore.release();
                    return;
                }
                mOfflinePageBridge.addObserver(new OfflinePageModelObserver() {
                    @Override
                    public void offlinePageModelLoaded() {
                        semaphore.release();
                        mOfflinePageBridge.removeObserver(this);
                    }
                });
            }
        });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Ensure we start in an offline state.
                NetworkChangeNotifier.forceConnectivityState(false);
                if (!NetworkChangeNotifier.isInitialized()) {
                    NetworkChangeNotifier.init();
                }
            }
        });

        initializeBridgeForProfile(false);

        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        mTestPage = mTestServer.getURL(TEST_PAGE);
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testLoadOfflinePagesWhenEmpty() throws Exception {
        List<OfflinePageItem> offlinePages = getAllPages();
        Assert.assertEquals("Offline pages count incorrect.", 0, offlinePages.size());
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testAddOfflinePageAndLoad() throws Exception {
        mActivityTestRule.loadUrl(mTestPage);
        savePage(SavePageResult.SUCCESS, mTestPage);
        List<OfflinePageItem> allPages = getAllPages();
        OfflinePageItem offlinePage = allPages.get(0);
        Assert.assertEquals("Offline pages count incorrect.", 1, allPages.size());
        Assert.assertEquals("Offline page item url incorrect.", mTestPage, offlinePage.getUrl());
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testGetPageByBookmarkId() throws Exception {
        mActivityTestRule.loadUrl(mTestPage);
        savePage(SavePageResult.SUCCESS, mTestPage);
        OfflinePageItem offlinePage = getPageByClientId(BOOKMARK_ID);
        Assert.assertEquals("Offline page item url incorrect.", mTestPage, offlinePage.getUrl());
        Assert.assertNull("Offline page is not supposed to exist",
                getPageByClientId(new ClientId(OfflinePageBridge.BOOKMARK_NAMESPACE, "-42")));
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testDeleteOfflinePage() throws Exception {
        deletePage(BOOKMARK_ID, DeletePageResult.SUCCESS);
        mActivityTestRule.loadUrl(mTestPage);
        savePage(SavePageResult.SUCCESS, mTestPage);
        Assert.assertNotNull(
                "Offline page should be available, but it is not.", getPageByClientId(BOOKMARK_ID));
        deletePage(BOOKMARK_ID, DeletePageResult.SUCCESS);
        Assert.assertNull("Offline page should be gone, but it is available.",
                getPageByClientId(BOOKMARK_ID));
    }

    @Test
    @CommandLineFlags.Add("disable-features=OfflinePagesSharing")
    @SmallTest
    @RetryOnFailure
    public void testPageSharingSwitch() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertFalse(
                        "If offline page sharing is off, we should see the feature disabled",
                        OfflinePageBridge.isPageSharingEnabled());
            }
        });
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testCheckPagesExistOffline() throws Exception {
        // If we save a page, then it should exist in the result.
        mActivityTestRule.loadUrl(mTestPage);
        savePage(SavePageResult.SUCCESS, mTestPage);
        Set<String> testCases = new HashSet<>();
        testCases.add(mTestPage);

        // Querying for a page that hasn't been saved should not affect the result.
        testCases.add(mTestPage + "?foo=bar");

        Set<String> pages = checkPagesExistOffline(testCases);

        Assert.assertEquals(
                "We only saved one page and queried for it, so the result should be one string.", 1,
                pages.size());
        Assert.assertTrue("The only page returned should be the page that was actually saved.",
                pages.contains(mTestPage));
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testGetRequestsInQueue() throws Exception {
        String url = "https://www.google.com/";
        String namespace = "custom_tabs";
        savePageLater(url, namespace);
        SavePageRequest[] requests = getRequestsInQueue();
        Assert.assertEquals(1, requests.length);
        Assert.assertEquals(namespace, requests[0].getClientId().getNamespace());
        Assert.assertEquals(url, requests[0].getUrl());

        String url2 = "https://mail.google.com/";
        String namespace2 = "last_n";
        savePageLater(url2, namespace2);
        requests = getRequestsInQueue();
        Assert.assertEquals(2, requests.length);

        HashSet<String> expectedUrls = new HashSet<>();
        expectedUrls.add(url);
        expectedUrls.add(url2);

        HashSet<String> expectedNamespaces = new HashSet<>();
        expectedNamespaces.add(namespace);
        expectedNamespaces.add(namespace2);

        for (SavePageRequest request : requests) {
            Assert.assertTrue(expectedNamespaces.contains(request.getClientId().getNamespace()));
            expectedNamespaces.remove(request.getClientId().getNamespace());
            Assert.assertTrue(expectedUrls.contains(request.getUrl()));
            expectedUrls.remove(request.getUrl());
        }
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testOfflinePageBridgeDisabledInIncognito() throws Exception {
        initializeBridgeForProfile(true);
        Assert.assertEquals(null, mOfflinePageBridge);
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testRemoveRequestsFromQueue() throws Exception {
        String url = "https://www.google.com/";
        String namespace = "custom_tabs";
        savePageLater(url, namespace);

        String url2 = "https://mail.google.com/";
        String namespace2 = "last_n";
        savePageLater(url2, namespace2);

        SavePageRequest[] requests = getRequestsInQueue();
        Assert.assertEquals(2, requests.length);

        List<Long> requestsToRemove = new ArrayList<>();
        requestsToRemove.add(Long.valueOf(requests[1].getRequestId()));

        List<OfflinePageBridge.RequestRemovedResult> removed =
                removeRequestsFromQueue(requestsToRemove);
        Assert.assertEquals(requests[1].getRequestId(), removed.get(0).getRequestId());
        Assert.assertEquals(
                org.chromium.components.offlinepages.background.UpdateRequestResult.SUCCESS,
                removed.get(0).getUpdateRequestResult());

        SavePageRequest[] remaining = getRequestsInQueue();
        Assert.assertEquals(1, remaining.length);

        Assert.assertEquals(requests[0].getRequestId(), remaining[0].getRequestId());
        Assert.assertEquals(requests[0].getUrl(), remaining[0].getUrl());
    }

    @Test
    @SmallTest
    public void testDeletePagesByOfflineIds() throws Exception {
        // Save 3 pages and record their offline IDs to delete later.
        Set<String> pageUrls = new HashSet<>();
        pageUrls.add(mTestPage);
        pageUrls.add(mTestPage + "?foo=1");
        pageUrls.add(mTestPage + "?foo=2");
        int pagesToDeleteCount = pageUrls.size();
        List<Long> offlineIdsToDelete = new ArrayList<>();
        for (String url : pageUrls) {
            mActivityTestRule.loadUrl(url);
            offlineIdsToDelete.add(savePage(SavePageResult.SUCCESS, url));
        }
        Assert.assertEquals("The pages should exist now that we saved them.", pagesToDeleteCount,
                checkPagesExistOffline(pageUrls).size());

        // Save one more page but don't save the offline ID, this page should not be deleted.
        Set<String> pageUrlsToSave = new HashSet<>();
        String pageToSave = mTestPage + "?bar=1";
        pageUrlsToSave.add(pageToSave);
        int pagesToSaveCount = pageUrlsToSave.size();
        for (String url : pageUrlsToSave) {
            mActivityTestRule.loadUrl(url);
            savePage(SavePageResult.SUCCESS, pageToSave);
        }
        Assert.assertEquals("The pages should exist now that we saved them.", pagesToSaveCount,
                checkPagesExistOffline(pageUrlsToSave).size());

        // Delete the first 3 pages.
        deletePages(offlineIdsToDelete);
        Assert.assertEquals(
                "The page should cease to exist.", 0, checkPagesExistOffline(pageUrls).size());

        // We should not have deleted the one we didn't ask to delete.
        Assert.assertEquals("The page should not be deleted.", pagesToSaveCount,
                checkPagesExistOffline(pageUrlsToSave).size());
    }

    @Test
    @SmallTest
    public void testGetPagesByNamespace() throws Exception {
        // Save 3 pages and record their offline IDs to delete later.
        Set<Long> offlineIdsToFetch = new HashSet<>();
        for (int i = 0; i < 3; i++) {
            String url = mTestPage + "?foo=" + i;
            mActivityTestRule.loadUrl(url);
            offlineIdsToFetch.add(savePage(SavePageResult.SUCCESS, url));
        }

        // Save a page in a different namespace.
        String urlToIgnore = mTestPage + "?bar=1";
        mActivityTestRule.loadUrl(urlToIgnore);
        long offlineIdToIgnore = savePage(SavePageResult.SUCCESS, urlToIgnore,
                new ClientId(OfflinePageBridge.ASYNC_NAMESPACE, "-42"));

        List<OfflinePageItem> pages = getPagesByNamespace(OfflinePageBridge.BOOKMARK_NAMESPACE);
        Assert.assertEquals(
                "The number of pages returned does not match the number of pages saved.",
                offlineIdsToFetch.size(), pages.size());
        for (OfflinePageItem page : pages) {
            offlineIdsToFetch.remove(page.getOfflineId());
        }
        Assert.assertEquals(
                "There were different pages saved than those returned by getPagesByNamespace.", 0,
                offlineIdsToFetch.size());

        // Check that the page in the other namespace still exists.
        List<OfflinePageItem> asyncPages = getPagesByNamespace(OfflinePageBridge.ASYNC_NAMESPACE);
        Assert.assertEquals("The page saved in an alternate namespace is no longer there.", 1,
                asyncPages.size());
        Assert.assertEquals(
                "The offline ID of the page saved in an alternate namespace does not match.",
                offlineIdToIgnore, asyncPages.get(0).getOfflineId());
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testDownloadPage() throws Exception {
        final OfflinePageOrigin origin =
                new OfflinePageOrigin("abc.xyz", new String[] {"deadbeef"});
        mActivityTestRule.loadUrl(mTestPage);
        final String originString = origin.encodeAsJsonString();
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertNotNull(
                        "Tab is null", mActivityTestRule.getActivity().getActivityTab());
                Assert.assertEquals("URL does not match requested.", mTestPage,
                        mActivityTestRule.getActivity().getActivityTab().getUrl());
                Assert.assertNotNull("WebContents is null",
                        mActivityTestRule.getActivity().getActivityTab().getWebContents());

                mOfflinePageBridge.addObserver(new OfflinePageModelObserver() {
                    @Override
                    public void offlinePageAdded(OfflinePageItem newPage) {
                        mOfflinePageBridge.removeObserver(this);
                        semaphore.release();
                    }
                });

                OfflinePageDownloadBridge.startDownload(
                        mActivityTestRule.getActivity().getActivityTab(), origin);
            }
        });
        Assert.assertTrue("Semaphore acquire failed. Timed out.",
                semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));

        List<OfflinePageItem> pages = getAllPages();
        Assert.assertEquals(originString, pages.get(0).getRequestOrigin());
    }

    @Test
    @SmallTest
    public void testSavePageWithRequestOrigin() throws Exception {
        final OfflinePageOrigin origin =
                new OfflinePageOrigin("abc.xyz", new String[] {"deadbeef"});
        mActivityTestRule.loadUrl(mTestPage);
        final String originString = origin.encodeAsJsonString();
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mOfflinePageBridge.addObserver(new OfflinePageModelObserver() {
                    @Override
                    public void offlinePageAdded(OfflinePageItem newPage) {
                        mOfflinePageBridge.removeObserver(this);
                        semaphore.release();
                    }
                });
                mOfflinePageBridge.savePage(
                        mActivityTestRule.getActivity().getActivityTab().getWebContents(),
                        BOOKMARK_ID, origin, new SavePageCallback() {
                            @Override
                            public void onSavePageDone(
                                    int savePageResult, String url, long offlineId) {}
                        });
            }
        });

        Assert.assertTrue("Semaphore acquire failed. Timed out.",
                semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        List<OfflinePageItem> pages = getAllPages();
        Assert.assertEquals(originString, pages.get(0).getRequestOrigin());
    }

    @Test
    @SmallTest
    public void testSavePageNoOrigin() throws Exception {
        mActivityTestRule.loadUrl(mTestPage);
        savePage(SavePageResult.SUCCESS, mTestPage);
        List<OfflinePageItem> pages = getAllPages();
        Assert.assertEquals("", pages.get(0).getRequestOrigin());
    }

    // Returns offline ID.
    private long savePage(final int expectedResult, final String expectedUrl)
            throws InterruptedException {
        return savePage(expectedResult, expectedUrl, BOOKMARK_ID);
    }

    // Returns offline ID.
    private long savePage(final int expectedResult, final String expectedUrl,
            final ClientId clientId) throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        final AtomicLong result = new AtomicLong(-1);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertNotNull(
                        "Tab is null", mActivityTestRule.getActivity().getActivityTab());
                Assert.assertEquals("URL does not match requested.", expectedUrl,
                        mActivityTestRule.getActivity().getActivityTab().getUrl());
                Assert.assertNotNull("WebContents is null",
                        mActivityTestRule.getActivity().getActivityTab().getWebContents());

                mOfflinePageBridge.savePage(
                        mActivityTestRule.getActivity().getActivityTab().getWebContents(), clientId,
                        new SavePageCallback() {
                            @Override
                            public void onSavePageDone(
                                    int savePageResult, String url, long offlineId) {
                                Assert.assertEquals(
                                        "Requested and returned URLs differ.", expectedUrl, url);
                                Assert.assertEquals(
                                        "Save result incorrect.", expectedResult, savePageResult);
                                result.set(offlineId);
                                semaphore.release();
                            }
                        });
            }
        });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        return result.get();
    }

    private void deletePages(final List<Long> offlineIds) throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mOfflinePageBridge.deletePagesByOfflineId(offlineIds, new Callback<Integer>() {
                    @Override
                    public void onResult(Integer deletePageResult) {
                        semaphore.release();
                    }
                });
            }
        });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    private void deletePage(final ClientId bookmarkId, final int expectedResult)
            throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        final AtomicInteger deletePageResultRef = new AtomicInteger();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mOfflinePageBridge.deletePage(bookmarkId, new Callback<Integer>() {
                    @Override
                    public void onResult(Integer deletePageResult) {
                        deletePageResultRef.set(deletePageResult.intValue());
                        semaphore.release();
                    }
                });
            }
        });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals("Delete result incorrect.", expectedResult, deletePageResultRef.get());
    }

    private List<OfflinePageItem> getAllPages() throws InterruptedException {
        final List<OfflinePageItem> result = new ArrayList<OfflinePageItem>();
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mOfflinePageBridge.getAllPages(new Callback<List<OfflinePageItem>>() {
                    @Override
                    public void onResult(List<OfflinePageItem> pages) {
                        result.addAll(pages);
                        semaphore.release();
                    }
                });
            }
        });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        return result;
    }

    private List<OfflinePageItem> getPagesByNamespace(final String namespace)
            throws InterruptedException {
        final List<OfflinePageItem> result = new ArrayList<OfflinePageItem>();
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mOfflinePageBridge.getPagesByNamespace(
                        namespace, new Callback<List<OfflinePageItem>>() {
                            @Override
                            public void onResult(List<OfflinePageItem> pages) {
                                result.addAll(pages);
                                semaphore.release();
                            }
                        });
            }
        });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
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

    private OfflinePageItem getPageByClientId(ClientId clientId) throws InterruptedException {
        final OfflinePageItem[] result = {null};
        final Semaphore semaphore = new Semaphore(0);
        final List<ClientId> clientIdList = new ArrayList<>();
        clientIdList.add(clientId);

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mOfflinePageBridge.getPagesByClientIds(
                        clientIdList, new Callback<List<OfflinePageItem>>() {
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
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        return result[0];
    }

    private Set<String> checkPagesExistOffline(final Set<String> query)
            throws InterruptedException {
        final Set<String> result = new HashSet<>();
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mOfflinePageBridge.checkPagesExistOffline(query, new Callback<Set<String>>() {
                    @Override
                    public void onResult(Set<String> offlinedPages) {
                        result.addAll(offlinedPages);
                        semaphore.release();
                    }
                });
            }
        });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        return result;
    }

    private void savePageLater(final String url, final String namespace)
            throws InterruptedException {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mOfflinePageBridge.savePageLater(url, namespace, true /* userRequested */);
            }
        });
    }

    private SavePageRequest[] getRequestsInQueue() throws InterruptedException {
        final AtomicReference<SavePageRequest[]> ref = new AtomicReference<>();
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mOfflinePageBridge.getRequestsInQueue(new Callback<SavePageRequest[]>() {
                    @Override
                    public void onResult(SavePageRequest[] requestsInQueue) {
                        ref.set(requestsInQueue);
                        semaphore.release();
                    }
                });
            }
        });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        return ref.get();
    }

    private List<OfflinePageBridge.RequestRemovedResult> removeRequestsFromQueue(
            final List<Long> requestsToRemove) throws InterruptedException {
        final AtomicReference<List<OfflinePageBridge.RequestRemovedResult>> ref =
                new AtomicReference<>();
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mOfflinePageBridge.removeRequestsFromQueue(requestsToRemove,
                        new Callback<List<OfflinePageBridge.RequestRemovedResult>>() {
                            @Override
                            public void onResult(
                                    List<OfflinePageBridge.RequestRemovedResult> removedRequests) {
                                ref.set(removedRequests);
                                semaphore.release();
                            }
                        });
            }
        });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        return ref.get();
    }
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.content.Context;
import android.support.test.filters.SmallTest;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.OfflinePageModelObserver;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.SavePageCallback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
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
import java.util.concurrent.atomic.AtomicReference;

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
        assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Ensure we start in an offline state.
                NetworkChangeNotifier.forceConnectivityState(false);
                Context context = getActivity().getBaseContext();
                if (!NetworkChangeNotifier.isInitialized()) {
                    NetworkChangeNotifier.init(context);
                }
            }
        });

        initializeBridgeForProfile(false);

        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
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
    @RetryOnFailure
    public void testLoadOfflinePagesWhenEmpty() throws Exception {
        List<OfflinePageItem> offlinePages = getAllPages();
        assertEquals("Offline pages count incorrect.", 0, offlinePages.size());
    }

    @SmallTest
    @RetryOnFailure
    public void testAddOfflinePageAndLoad() throws Exception {
        loadUrl(mTestPage);
        savePage(SavePageResult.SUCCESS, mTestPage);
        List<OfflinePageItem> allPages = getAllPages();
        OfflinePageItem offlinePage = allPages.get(0);
        assertEquals("Offline pages count incorrect.", 1, allPages.size());
        assertEquals("Offline page item url incorrect.", mTestPage, offlinePage.getUrl());

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
    @RetryOnFailure
    public void testGetPageByBookmarkId() throws Exception {
        loadUrl(mTestPage);
        savePage(SavePageResult.SUCCESS, mTestPage);
        OfflinePageItem offlinePage = getPageByClientId(BOOKMARK_ID);
        assertEquals("Offline page item url incorrect.", mTestPage, offlinePage.getUrl());
        assertNull("Offline page is not supposed to exist",
                getPageByClientId(new ClientId(OfflinePageBridge.BOOKMARK_NAMESPACE, "-42")));
    }

    @SmallTest
    @RetryOnFailure
    public void testDeleteOfflinePage() throws Exception {
        deletePage(BOOKMARK_ID, DeletePageResult.SUCCESS);
        loadUrl(mTestPage);
        savePage(SavePageResult.SUCCESS, mTestPage);
        assertNotNull(
                "Offline page should be available, but it is not.", getPageByClientId(BOOKMARK_ID));
        deletePage(BOOKMARK_ID, DeletePageResult.SUCCESS);
        assertNull("Offline page should be gone, but it is available.",
                getPageByClientId(BOOKMARK_ID));
    }

    @CommandLineFlags.Add("disable-features=OfflinePagesSharing")
    @SmallTest
    @RetryOnFailure
    public void testPageSharingSwitch() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertFalse("If offline page sharing is off, we should see the feature disabled",
                        OfflinePageBridge.isPageSharingEnabled());
            }
        });
    }

    @SmallTest
    @RetryOnFailure
    public void testCheckPagesExistOffline() throws Exception {
        // If we save a page, then it should exist in the result.
        loadUrl(mTestPage);
        savePage(SavePageResult.SUCCESS, mTestPage);
        Set<String> testCases = new HashSet<>();
        testCases.add(mTestPage);

        // Querying for a page that hasn't been saved should not affect the result.
        testCases.add(mTestPage + "?foo=bar");

        Set<String> pages = checkPagesExistOffline(testCases);

        assertEquals(
                "We only saved one page and queried for it, so the result should be one string.", 1,
                pages.size());
        assertTrue("The only page returned should be the page that was actually saved.",
                pages.contains(mTestPage));
    }

    @SmallTest
    @RetryOnFailure
    public void testGetRequestsInQueue() throws Exception {
        String url = "https://www.google.com/";
        String namespace = "custom_tabs";
        savePageLater(url, namespace);
        SavePageRequest[] requests = getRequestsInQueue();
        assertEquals(1, requests.length);
        assertEquals(namespace, requests[0].getClientId().getNamespace());
        assertEquals(url, requests[0].getUrl());

        String url2 = "https://mail.google.com/";
        String namespace2 = "last_n";
        savePageLater(url2, namespace2);
        requests = getRequestsInQueue();
        assertEquals(2, requests.length);

        HashSet<String> expectedUrls = new HashSet<>();
        expectedUrls.add(url);
        expectedUrls.add(url2);

        HashSet<String> expectedNamespaces = new HashSet<>();
        expectedNamespaces.add(namespace);
        expectedNamespaces.add(namespace2);

        for (SavePageRequest request : requests) {
            assertTrue(expectedNamespaces.contains(request.getClientId().getNamespace()));
            expectedNamespaces.remove(request.getClientId().getNamespace());
            assertTrue(expectedUrls.contains(request.getUrl()));
            expectedUrls.remove(request.getUrl());
        }
    }

    @SmallTest
    @RetryOnFailure
    public void testOfflinePageBridgeDisabledInIncognito() throws Exception {
        initializeBridgeForProfile(true);
        assertEquals(null, mOfflinePageBridge);
    }

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
        assertEquals(2, requests.length);

        List<Long> requestsToRemove = new ArrayList<>();
        requestsToRemove.add(Long.valueOf(requests[1].getRequestId()));

        List<OfflinePageBridge.RequestRemovedResult> removed =
                removeRequestsFromQueue(requestsToRemove);
        assertEquals(requests[1].getRequestId(), removed.get(0).getRequestId());
        assertEquals(org.chromium.components.offlinepages.background.UpdateRequestResult.SUCCESS,
                removed.get(0).getUpdateRequestResult());

        SavePageRequest[] remaining = getRequestsInQueue();
        assertEquals(1, remaining.length);

        assertEquals(requests[0].getRequestId(), remaining[0].getRequestId());
        assertEquals(requests[0].getUrl(), remaining[0].getUrl());
    }

    private void savePage(final int expectedResult, final String expectedUrl)
            throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertNotNull("Tab is null", getActivity().getActivityTab());
                assertEquals("URL does not match requested.", expectedUrl,
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
                mOfflinePageBridge.deletePage(bookmarkId, new Callback<Integer>() {
                    @Override
                    public void onResult(Integer deletePageResult) {
                        deletePageResultRef.set(deletePageResult.intValue());
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
                mOfflinePageBridge.getAllPages(new Callback<List<OfflinePageItem>>() {
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
        assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
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
        assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
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
        assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
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
        assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        return ref.get();
    }
}

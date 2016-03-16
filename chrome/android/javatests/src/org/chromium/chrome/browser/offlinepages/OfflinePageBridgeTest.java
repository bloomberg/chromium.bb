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
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.DeletePageCallback;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.OfflinePageModelObserver;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.SavePageCallback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.components.offlinepages.DeletePageResult;
import org.chromium.components.offlinepages.SavePageResult;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for {@link OfflinePageBridge}. */
@CommandLineFlags.Add({ChromeSwitches.ENABLE_OFFLINE_PAGES})
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
    public void testGetLaunchUrlFromOnlineUrl() throws Exception {
        // Start online
        forceConnectivityStateOnUiThread(true);
        loadUrl(mTestPage);
        savePage(SavePageResult.SUCCESS, mTestPage);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Should return online URL since we are online.
                assertEquals(mTestPage, mOfflinePageBridge.getLaunchUrlFromOnlineUrl(mTestPage));

                // Switch to offline
                NetworkChangeNotifier.forceConnectivityState(false);

                // Should return saved page URL since we are offline.
                assertTrue("Offline page item offline file url doesn't start properly.",
                        mOfflinePageBridge.getLaunchUrlFromOnlineUrl(mTestPage).startsWith(
                                "file:///"));
            }
        });
    }

    @SmallTest
    public void testGetLaunchUrlAndMarkAccessed() throws Exception {
        // Start online
        forceConnectivityStateOnUiThread(true);

        loadUrl(mTestPage);
        savePage(SavePageResult.SUCCESS, mTestPage);

        final AtomicReference<OfflinePageItem> offlinePageRef = new AtomicReference<>();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                OfflinePageItem offlinePage = mOfflinePageBridge.getPageByClientId(BOOKMARK_ID);
                offlinePageRef.set(offlinePage);
                assertEquals("", 0, offlinePage.getAccessCount());
                long initialAccessTimeMs = offlinePage.getLastAccessTimeMs();

                assertEquals("Should return online URL while online", mTestPage,
                        mOfflinePageBridge.getLaunchUrlAndMarkAccessed(offlinePage, mTestPage));

                assertEquals("Get launch URL should not affect access time while online.",
                        initialAccessTimeMs,
                        mOfflinePageBridge.getPageByClientId(BOOKMARK_ID).getLastAccessTimeMs());
                assertEquals("Get launch URL should not affect access count while online.", 0,
                        mOfflinePageBridge.getPageByClientId(BOOKMARK_ID).getAccessCount());

                // Switch to offline
                NetworkChangeNotifier.forceConnectivityState(false);

                // Should return saved page URL since we are offline.
                assertTrue("Offline page item offline file url doesn't start properly.",
                        mOfflinePageBridge.getLaunchUrlAndMarkAccessed(offlinePage, mTestPage)
                                .startsWith("file:///"));
            }
        });

        // We need to poll since there is no callback for mark page as accessed.
        try {
            CriteriaHelper.pollUiThread(
                    new Criteria("Failed while waiting for access count to change.") {
                        @Override
                        public boolean isSatisfied() {
                            OfflinePageItem entry =
                                    mOfflinePageBridge.getPageByClientId(BOOKMARK_ID);
                            return entry.getAccessCount() != 0;
                        }
                    },
                    TIMEOUT_MS, POLLING_INTERVAL);
        } catch (InterruptedException e) {
            fail("Failed while waiting for access count to change." + e);
        }

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                OfflinePageItem entry = mOfflinePageBridge.getPageByClientId(BOOKMARK_ID);
                assertEquals(
                        "GetLaunchUrl should increment accessed count when used while offline.", 1,
                        entry.getAccessCount());

                assertTrue("GetLaunchUrl should update last accessed time when used while offline.",
                        entry.getLastAccessTimeMs() > offlinePageRef.get().getLastAccessTimeMs());
            }
        });
    }

    @SmallTest
    public void testGetPageByBookmarkId() throws Exception {
        loadUrl(mTestPage);
        savePage(SavePageResult.SUCCESS, mTestPage);
        OfflinePageItem offlinePage = mOfflinePageBridge.getPageByClientId(BOOKMARK_ID);
        assertEquals("Offline page item url incorrect.", mTestPage, offlinePage.getUrl());
        assertTrue("Offline page item offline file url doesn't start properly.",
                offlinePage.getOfflineUrl().startsWith("file:///"));
        assertTrue("Offline page item offline file doesn't have the right name.",
                offlinePage.getOfflineUrl().endsWith(".mhtml"));
        assertTrue("Offline page item offline file doesn't have the right name.",
                offlinePage.getOfflineUrl().contains("About"));

        assertNull("Offline page is not supposed to exist",
                mOfflinePageBridge.getPageByClientId(
                        new ClientId(OfflinePageBridge.BOOKMARK_NAMESPACE, "-42")));
    }

    @SmallTest
    public void testDeleteOfflinePage() throws Exception {
        deletePage(BOOKMARK_ID, DeletePageResult.NOT_FOUND);
        loadUrl(mTestPage);
        savePage(SavePageResult.SUCCESS, mTestPage);
        assertNotNull("Offline page should be available, but it is not.",
                mOfflinePageBridge.getPageByClientId(BOOKMARK_ID));
        deletePage(BOOKMARK_ID, DeletePageResult.SUCCESS);
        assertNull("Offline page should be gone, but it is available.",
                mOfflinePageBridge.getPageByClientId(BOOKMARK_ID));
    }

    @SmallTest
    public void testGetOfflineUrlForOnlineUrl() throws Exception {
        loadUrl(mTestPage);
        savePage(SavePageResult.SUCCESS, mTestPage);
        OfflinePageItem offlinePage = mOfflinePageBridge.getPageByClientId(BOOKMARK_ID);
        assertEquals("We should get the same offline URL, when querying using online URL",
                offlinePage.getOfflineUrl(),
                mOfflinePageBridge.getOfflineUrlForOnlineUrl(offlinePage.getUrl()));
    }

    @CommandLineFlags.Add("disable-features=offline-pages-background-loading")
    @SmallTest
    public void testBackgroundLoadSwitch() throws Exception {
        // We should be able to call the C++ is enabled function from the Java side.
        assertFalse("If background loading is off, we should see the feature disabled",
                OfflinePageBridge.isBackgroundLoadingEnabled());
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
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mOfflinePageBridge.deletePage(bookmarkId, new DeletePageCallback() {
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
        final List<OfflinePageItem> result = new ArrayList<OfflinePageItem>();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                result.clear();
                for (OfflinePageItem item : mOfflinePageBridge.getAllPages()) {
                    result.add(item);
                }
            }
        });
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
}

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.app.Activity;
import android.test.suitebuilder.annotation.LargeTest;
import android.util.Log;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.shell.ChromeShellActivity;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.sync.AndroidSyncSettings;
import org.chromium.sync.internal_api.pub.base.ModelType;
import org.chromium.sync.protocol.EntitySpecifics;
import org.chromium.sync.protocol.SyncEnums;
import org.chromium.sync.protocol.TypedUrlSpecifics;
import org.chromium.sync.signin.AccountManagerHelper;
import org.chromium.sync.signin.ChromeSigninController;
import org.chromium.ui.base.PageTransition;

import java.util.Map;
import java.util.concurrent.TimeoutException;

/**
 * Test suite for Sync.
 */
public class SyncTest extends SyncTestBase {
    private static final String TAG = "SyncTest";

    /**
     * This is a regression test for http://crbug.com/475299.
     */
    @LargeTest
    @Feature({"Sync"})
    public void testGcmInitialized() throws Throwable {
        setupTestAccountAndSignInToSync(CLIENT_ID);
        assertTrue(ChromeSigninController.get(mContext).isGcmInitialized());
    }

    @LargeTest
    @Feature({"Sync"})
    public void testGetAboutSyncInfoYieldsValidData() throws Throwable {
        setupTestAccountAndSignInToSync(CLIENT_ID);

        final SyncTestUtil.AboutSyncInfoGetter syncInfoGetter =
                new SyncTestUtil.AboutSyncInfoGetter(getActivity());
        runTestOnUiThread(syncInfoGetter);

        boolean gotInfo = CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !syncInfoGetter.getAboutInfo().isEmpty();
            }
        }, SyncTestUtil.UI_TIMEOUT_MS, SyncTestUtil.CHECK_INTERVAL_MS);

        assertTrue("Couldn't get about info.", gotInfo);
    }

    @LargeTest
    @Feature({"Sync"})
    public void testFlushDirectoryDoesntBreakSync() throws Throwable {
        setupTestAccountAndSignInToSync(CLIENT_ID);
        final Activity activity = getActivity();

        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                ApplicationStatus.onStateChangeForTesting(activity, ActivityState.PAUSED);
            }
        });

        // TODO(pvalenzuela): When available, check that sync is still functional.
    }

    @LargeTest
    @Feature({"Sync"})
    public void testAboutSyncPageDisplaysCurrentSyncStatus() throws InterruptedException {
        setupTestAccountAndSignInToSync(CLIENT_ID);

        loadUrlWithSanitization("chrome://sync");
        SyncTestUtil.AboutSyncInfoGetter aboutInfoGetter =
                new SyncTestUtil.AboutSyncInfoGetter(getActivity());
        try {
            runTestOnUiThread(aboutInfoGetter);
        } catch (Throwable t) {
            Log.w(TAG,
                    "Exception while trying to fetch about sync info from ProfileSyncService.", t);
            fail("Unable to fetch sync info from ProfileSyncService.");
        }
        assertFalse("About sync info should not be empty.",
                aboutInfoGetter.getAboutInfo().isEmpty());
        assertTrue("About sync info should have sync summary status.",
                aboutInfoGetter.getAboutInfo().containsKey(SyncTestUtil.SYNC_SUMMARY_STATUS));
        final String expectedSyncSummary =
                aboutInfoGetter.getAboutInfo().get(SyncTestUtil.SYNC_SUMMARY_STATUS);

        Criteria checker = new Criteria() {
            @Override
            public boolean isSatisfied() {
                final ContentViewCore contentViewCore = getContentViewCore(getActivity());
                String innerHtml = "";
                try {
                    innerHtml = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                            contentViewCore.getWebContents(), "document.documentElement.innerHTML");
                } catch (InterruptedException e) {
                    Log.w(TAG, "Interrupted while polling about:sync page for sync status.", e);
                } catch (TimeoutException e) {
                    Log.w(TAG, "Interrupted while polling about:sync page for sync status.", e);
                }
                return innerHtml.contains(expectedSyncSummary);
            }

        };
        boolean hadExpectedStatus = CriteriaHelper.pollForCriteria(
                checker, SyncTestUtil.UI_TIMEOUT_MS, SyncTestUtil.CHECK_INTERVAL_MS);
        assertTrue("Sync status not present on about sync page: " + expectedSyncSummary,
                hadExpectedStatus);
    }

    @LargeTest
    @Feature({"Sync"})
    public void testDisableAndEnableSync() throws InterruptedException {
        setupTestAccountAndSignInToSync(CLIENT_ID);
        Account account =
                AccountManagerHelper.createAccountFromName(SyncTestUtil.DEFAULT_TEST_ACCOUNT);

        // Disabling Android sync should turn Chrome sync engine off.
        AndroidSyncSettings.get(mContext).disableChromeSync();
        SyncTestUtil.verifySyncIsDisabled(mContext, account);

        // Enabling Android sync should turn Chrome sync engine on.
        AndroidSyncSettings.get(mContext).enableChromeSync();
        SyncTestUtil.ensureSyncInitialized(mContext);
        SyncTestUtil.verifySignedInWithAccount(mContext, account);
    }

    @LargeTest
    @Feature({"Sync"})
    public void testUploadTypedUrl() throws Exception {
        setupTestAccountAndSignInToSync(CLIENT_ID);

        // TestHttpServerClient is preferred here but it can't be used. The test server
        // serves pages on localhost and Chrome doesn't sync localhost URLs as typed URLs.
        // This type of URL requires no external data connection or resources.
        final String urlToLoad = "data:text,testTypedUrl";
        assertTrue("A typed URL entity for " + urlToLoad + " already exists on the fake server.",
                mFakeServerHelper.verifyEntityCountByTypeAndName(0, ModelType.TYPED_URL,
                        urlToLoad));

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                LoadUrlParams params = new LoadUrlParams(urlToLoad, PageTransition.TYPED);
                getActivity().getActiveTab().loadUrl(params);
            }
        });

        boolean synced = CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mFakeServerHelper.verifyEntityCountByTypeAndName(1, ModelType.TYPED_URL,
                        urlToLoad);
            }
        }, SyncTestUtil.UI_TIMEOUT_MS, SyncTestUtil.CHECK_INTERVAL_MS);

        assertTrue("The typed URL entity for " + urlToLoad + " was not found on the fake server.",
                synced);
    }

    /**
     * Retrieves a local entity count and asserts that {@code expected} entities exist on the client
     * with the ModelType represented by {@code modelTypeString}.
     *
     * TODO(pvalenzuela): Replace modelTypeString with the native ModelType enum or something else
     * that will avoid callers needing to specify the native string version.
     */
    private void assertLocalEntityCount(String modelTypeString, int expected)
            throws InterruptedException {
        final SyncTestUtil.AboutSyncInfoGetter aboutInfoGetter =
                new SyncTestUtil.AboutSyncInfoGetter(getActivity());
        try {
            runTestOnUiThread(aboutInfoGetter);
        } catch (Throwable t) {
            Log.w(TAG,
                    "Exception while trying to fetch about sync info from ProfileSyncService.", t);
            fail("Unable to fetch sync info from ProfileSyncService.");
        }
        boolean receivedModelTypeCounts = CriteriaHelper.pollForCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return !aboutInfoGetter.getModelTypeCount().isEmpty();
                }
        }, SyncTestUtil.UI_TIMEOUT_MS, SyncTestUtil.CHECK_INTERVAL_MS);
        assertTrue("No model type counts present. Sync might be disabled.",
                receivedModelTypeCounts);

        Map<String, Integer> modelTypeCount = aboutInfoGetter.getModelTypeCount();
        assertTrue("No count for model type: " + modelTypeString,
                modelTypeCount.containsKey(modelTypeString));

        // Reduce by one to account for type's root entity. This entity is always included but
        // these tests don't care about its existence.
        int actual = modelTypeCount.get(modelTypeString) - 1;
        assertEquals("Expected amount of local client entities did not match.", expected, actual);
    }

    @LargeTest
    @Feature({"Sync"})
    public void testDownloadTypedUrl() throws InterruptedException {
        setupTestAccountAndSignInToSync(CLIENT_ID);
        assertLocalEntityCount("Typed URLs", 0);

        String url = "data:text,testDownloadTypedUrl";
        EntitySpecifics specifics = new EntitySpecifics();
        specifics.typedUrl = new TypedUrlSpecifics();
        specifics.typedUrl.url = url;
        specifics.typedUrl.title = url;
        specifics.typedUrl.visits = new long[]{1L};
        specifics.typedUrl.visitTransitions = new int[]{SyncEnums.TYPED};
        mFakeServerHelper.injectUniqueClientEntity(url /* name */, specifics);

        SyncTestUtil.triggerSyncAndWaitForCompletion(mContext);
        assertLocalEntityCount("Typed URLs", 1);

        // TODO(pvalenzuela): Also verify that the downloaded typed URL matches the one that was
        // injected. This data should be retrieved from the Sync node browser data.
    }

    @LargeTest
    @Feature({"Sync"})
    public void testDownloadBookmark() throws InterruptedException {
        setupTestAccountAndSignInToSync(CLIENT_ID);
        // 3 bookmark folders exist by default: Bookmarks Bar, Other Bookmarks, Mobile Bookmarks.
        assertLocalEntityCount("Bookmarks", 3);

        mFakeServerHelper.injectBookmarkEntity(
                "Title", "http://chromium.org", mFakeServerHelper.getBookmarkBarFolderId());

        SyncTestUtil.triggerSyncAndWaitForCompletion(mContext);
        assertLocalEntityCount("Bookmarks", 4);

        // TODO(pvalenzuela): Also verify that the downloaded bookmark matches the one that was
        // injected. This data should be retrieved from the Sync node browser data.
    }

    private static ContentViewCore getContentViewCore(ChromeShellActivity activity) {
        return activity.getActiveContentViewCore();
    }
}

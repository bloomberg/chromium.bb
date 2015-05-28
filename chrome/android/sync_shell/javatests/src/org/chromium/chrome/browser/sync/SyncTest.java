// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.app.Activity;
import android.test.suitebuilder.annotation.LargeTest;
import android.util.Log;
import android.util.Pair;

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
import org.json.JSONObject;

import java.util.List;
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
    public void testSignInAndOut() throws InterruptedException {
        setupTestAccountAndSignInToSync(CLIENT_ID);
        Account account =
                AccountManagerHelper.createAccountFromName(SyncTestUtil.DEFAULT_TEST_ACCOUNT);

        // Signing out should disable sync.
        signOut();
        SyncTestUtil.verifySyncIsSignedOut(mContext);

        // Signing back in should re-enable sync.
        signIn(account);
        SyncTestUtil.verifySyncIsSignedIn(mContext, account);
    }

    @LargeTest
    @Feature({"Sync"})
    public void testStopAndStartSync() throws InterruptedException {
        setupTestAccountAndSignInToSync(CLIENT_ID);
        Account account =
                AccountManagerHelper.createAccountFromName(SyncTestUtil.DEFAULT_TEST_ACCOUNT);

        SyncTestUtil.verifySyncIsSignedIn(mContext, account);
        stopSync();
        SyncTestUtil.verifySyncIsDisabled(mContext, account);
        startSync();
        SyncTestUtil.verifySyncIsSignedIn(mContext, account);
    }

    @LargeTest
    @Feature({"Sync"})
    public void testDisableAndEnableSyncThroughAndroid() throws InterruptedException {
        setupTestAccountAndSignInToSync(CLIENT_ID);
        SyncTestUtil.ensureSyncInitialized(mContext);

        Account account =
                AccountManagerHelper.createAccountFromName(SyncTestUtil.DEFAULT_TEST_ACCOUNT);
        String authority = AndroidSyncSettings.getContractAuthority(mContext);

        // Disabling Android sync should turn Chrome sync engine off.
        mSyncContentResolver.setSyncAutomatically(account, authority, false);
        SyncTestUtil.verifySyncIsDisabled(mContext, account);

        // Enabling Android sync should turn Chrome sync engine on.
        mSyncContentResolver.setSyncAutomatically(account, authority, true);
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

    @LargeTest
    @Feature({"Sync"})
    public void testDownloadTypedUrl() throws Exception {
        setupTestAccountAndSignInToSync(CLIENT_ID);
        assertEquals("No typed URLs should exist on the client by default.",
                0, SyncTestUtil.getLocalData(mContext, "Typed URLs").size());

        String url = "data:text,testDownloadTypedUrl";
        EntitySpecifics specifics = new EntitySpecifics();
        specifics.typedUrl = new TypedUrlSpecifics();
        specifics.typedUrl.url = url;
        specifics.typedUrl.title = url;
        specifics.typedUrl.visits = new long[]{1L};
        specifics.typedUrl.visitTransitions = new int[]{SyncEnums.TYPED};
        mFakeServerHelper.injectUniqueClientEntity(url /* name */, specifics);

        SyncTestUtil.triggerSyncAndWaitForCompletion(mContext);

        List<Pair<String, JSONObject>> typedUrls = SyncTestUtil.getLocalData(
                mContext, "Typed URLs");
        assertEquals("Only the injected typed URL should exist on the client.",
                1, typedUrls.size());
        JSONObject typedUrl = typedUrls.get(0).second;
        assertEquals("The wrong URL was found for the typed URL.", url, typedUrl.getString("url"));
    }

    private static ContentViewCore getContentViewCore(ChromeShellActivity activity) {
        return activity.getActiveContentViewCore();
    }
}

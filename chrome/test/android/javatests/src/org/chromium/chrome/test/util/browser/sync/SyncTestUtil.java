// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.sync;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.accounts.Account;
import android.content.Context;
import android.util.Log;
import android.util.Pair;

import junit.framework.Assert;

import org.chromium.base.CommandLine;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.sync.signin.AccountManagerHelper;
import org.chromium.sync.signin.ChromeSigninController;
import org.chromium.sync.test.util.AccountHolder;
import org.chromium.sync.test.util.MockAccountManager;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Utility class for shared sync test functionality.
 */
public final class SyncTestUtil {

    public static final String DEFAULT_TEST_ACCOUNT = "test@gmail.com";
    public static final String DEFAULT_PASSWORD = "myPassword";
    private static final String TAG = "SyncTestUtil";

    public static final long UI_TIMEOUT_MS = scaleTimeout(20000);
    public static final int CHECK_INTERVAL_MS = 250;

    private static final long SYNC_WAIT_TIMEOUT_MS = scaleTimeout(30 * 1000);
    private static final int SYNC_CHECK_INTERVAL_MS = 250;

    public static final Pair<String, String> SYNC_SUMMARY_STATUS =
            newPair("Summary", "Summary");
    protected static final String UNINITIALIZED = "Uninitialized";
    protected static final Pair<String, String> USERNAME_STAT =
            newPair("Identity", "Username");

    // Override the default server used for profile sync.
    // Native switch - chrome_switches::kSyncServiceURL
    private static final String SYNC_URL = "sync-url";

    private SyncTestUtil() {
    }

    /**
     * Creates a Pair of lowercased and trimmed Strings. Makes it easier to avoid running afoul of
     * case-sensitive comparison since getAboutInfoStats(), et al, use Pair<String, String> as map
     * keys.
     */
    private static Pair<String, String> newPair(String first, String second) {
        return Pair.create(first.toLowerCase(Locale.US).trim(),
                second.toLowerCase(Locale.US).trim());
    }

    /**
     * Parses raw JSON into a map with keys Pair<String, String>. The first string in each Pair
     * corresponds to the title under which a given stat_name/stat_value is situated, and the second
     * contains the name of the actual stat. For example, a stat named "Syncing" which falls under
     * "Local State" would be a Pair of newPair("Local State", "Syncing").
     *
     * @param rawJson the JSON to parse into a map
     * @return a map containing a mapping of titles and stat names to stat values
     * @throws org.json.JSONException
     */
    public static Map<Pair<String, String>, String> getAboutInfoStats(String rawJson)
            throws JSONException {

        // What we get back is what you'd get from chrome.sync.aboutInfo at chrome://sync. This is
        // a JSON object, and we care about the "details" field in that object. "details" itself has
        // objects with two fields: data and title. The data field itself contains an array of
        // objects. These objects contains two fields: stat_name and stat_value. Ultimately these
        // are the values displayed on the page and the values we care about in this method.
        Map<Pair<String, String>, String> statLookup = new HashMap<Pair<String, String>, String>();
        JSONObject aboutInfo = new JSONObject(rawJson);
        JSONArray detailsArray = aboutInfo.getJSONArray("details");
        for (int i = 0; i < detailsArray.length(); i++) {
            JSONObject dataObj = detailsArray.getJSONObject(i);
            String dataTitle = dataObj.getString("title");
            JSONArray dataArray = dataObj.getJSONArray("data");
            for (int j = 0; j < dataArray.length(); j++) {
                JSONObject statObj = dataArray.getJSONObject(j);
                String statName = statObj.getString("stat_name");
                Pair<String, String> key = newPair(dataTitle, statName);
                statLookup.put(key, statObj.getString("stat_value"));
            }
        }

        return statLookup;
    }

    /**
     * Verifies that sync is signed out and its status is "Syncing not enabled".
     * TODO(mmontgomery): check whether or not this method is necessary. It queries
     * syncSummaryStatus(), which is a slightly more direct route than via JSON.
     */
    public static void verifySyncIsSignedOut(Context context) {
        Map<Pair<String, String>, String> expectedStats =
                new HashMap<Pair<String, String>, String>();
        expectedStats.put(SYNC_SUMMARY_STATUS, UNINITIALIZED);
        expectedStats.put(USERNAME_STAT, ""); // Expect an empty username when sync is signed out.
        Assert.assertTrue("Expected sync to be disabled.",
                pollAboutSyncStats(context, expectedStats));
    }

    /**
     * Polls the stats on about:sync until timeout or all expected stats match actual stats. The
     * comparison is case insensitive. *All* stats must match those passed in via expectedStats.
     *
     *
     * @param expectedStats a map of stat names to their expected values
     * @return whether the stats matched up before the timeout
     */
    public static boolean pollAboutSyncStats(
            Context context, final Map<Pair<String, String>, String> expectedStats) {
        final AboutSyncInfoGetter aboutInfoGetter =
                new AboutSyncInfoGetter(context);

        Criteria statChecker = new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    ThreadUtils.runOnUiThreadBlocking(aboutInfoGetter);
                    Map<Pair<String, String>, String> actualStats = aboutInfoGetter.getAboutInfo();
                    return areExpectedStatsAmongActual(expectedStats, actualStats);
                } catch (Throwable e) {
                    Log.w(TAG, "Interrupted while attempting to fetch sync internals info.", e);
                }
                return false;
            }
        };

        boolean matched = false;
        try {
            matched = CriteriaHelper.pollForCriteria(statChecker, UI_TIMEOUT_MS, CHECK_INTERVAL_MS);
        } catch (InterruptedException e) {
            Log.w(TAG, "Interrupted while polling sync internals info.", e);
            Assert.fail("Interrupted while polling sync internals info.");
        }
        return matched;
    }

    /**
     * Checks whether the expected map's keys and values are a subset of those in another map. Both
     * keys and values are compared in a case-insensitive fashion.
     *
     * @param expectedStats a map which may be a subset of actualSet
     * @param actualStats   a map which may be a superset of expectedSet
     * @return true if all key/value pairs in expectedSet are in actualSet; false otherwise
     */
    private static boolean areExpectedStatsAmongActual(
            Map<Pair<String, String>, String> expectedStats,
            Map<Pair<String, String>, String> actualStats) {
        for (Map.Entry<Pair<String, String>, String> statEntry : expectedStats.entrySet()) {
            // Make stuff lowercase here, at the site of comparison.
            String expectedValue = statEntry.getValue().toLowerCase(Locale.US).trim();
            String actualValue = actualStats.get(statEntry.getKey());
            if (actualValue == null) {
                return false;
            }
            actualValue = actualValue.toLowerCase(Locale.US).trim();
            if (!expectedValue.contentEquals(actualValue)) {
                return false;
            }
        }
        return true;
    }

    /**
     * Triggers a sync and waits till it is complete.
     */
    public static void triggerSyncAndWaitForCompletion(final Context context)
            throws InterruptedException {
        final long oldSyncTime = getCurrentSyncTime(context);
        // Request sync.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ProfileSyncService.get(context).requestSyncCycleForTest();
            }
        });

        // Wait till lastSyncedTime > oldSyncTime.
        Assert.assertTrue("Timed out waiting for syncing to complete.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        long currentSyncTime = 0;
                        try {
                            currentSyncTime = getCurrentSyncTime(context);
                        } catch (InterruptedException e) {
                            Log.w(TAG, "Interrupted while getting sync time.", e);
                        }
                        return currentSyncTime > oldSyncTime;
                    }
                }, SYNC_WAIT_TIMEOUT_MS, SYNC_CHECK_INTERVAL_MS));
    }

    private static long getCurrentSyncTime(final Context context) throws InterruptedException {
        final Semaphore s = new Semaphore(0);
        final AtomicLong result = new AtomicLong();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                result.set(ProfileSyncService.get(context).getLastSyncedTimeForTest());
                s.release();
            }
        });
        Assert.assertTrue(s.tryAcquire(SYNC_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        return result.get();
    }

    /**
     * Waits for a possible async initialization of the sync backend.
     */
    public static void ensureSyncInitialized(final Context context) throws InterruptedException {
        Assert.assertTrue("Timed out waiting for syncing to be initialized.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return ThreadUtils.runOnUiThreadBlockingNoException(
                                new Callable<Boolean>() {
                                    @Override
                                    public Boolean call() throws Exception {
                                        return ProfileSyncService.get(context)
                                                .isSyncInitialized();

                                    }
                                });
                    }
                }, SYNC_WAIT_TIMEOUT_MS, SYNC_CHECK_INTERVAL_MS));
    }

    /**
     * Verifies that the sync status is "READY" and sync is signed in with the account.
     */
    public static void verifySyncIsSignedIn(Context context, Account account)
            throws InterruptedException {
        ensureSyncInitialized(context);
        triggerSyncAndWaitForCompletion(context);
        verifySignedInWithAccount(context, account);
    }

    /**
     * Makes sure that sync is enabled with the correct account.
     */
    public static void verifySignedInWithAccount(Context context, Account account) {
        if (account == null) return;

        Assert.assertEquals(
                account.name, ChromeSigninController.get(context).getSignedInAccountName());
    }

    /**
     * Makes sure that the Python sync server was successfully started by checking for a well known
     * response to a request for the server time. The command line argument for the sync server must
     * be present in order for this check to be valid.
     */
    public static void verifySyncServerIsRunning() {
        boolean hasSwitch = CommandLine.getInstance().hasSwitch(SYNC_URL);
        Assert.assertTrue(SYNC_URL + " is a required parameter for the sync tests.", hasSwitch);
        String syncTimeUrl = CommandLine.getInstance().getSwitchValue(SYNC_URL) + "/time";
        TestHttpServerClient.checkServerIsUp(syncTimeUrl, "0123456789");
    }

    /**
     * Sets up a test Google account on the device with specified auth token types.
     */
    public static Account setupTestAccount(MockAccountManager accountManager, String accountName,
                                           String password, String... allowedAuthTokenTypes) {
        Account account = AccountManagerHelper.createAccountFromName(accountName);
        AccountHolder.Builder accountHolder =
                AccountHolder.create().account(account).password(password);
        if (allowedAuthTokenTypes != null) {
            // Auto-allowing provided auth token types
            for (String authTokenType : allowedAuthTokenTypes) {
                accountHolder.hasBeenAccepted(authTokenType, true);
            }
        }
        accountManager.addAccountHolderExplicitly(accountHolder.build());
        return account;
    }

    /**
     * Sets up a test Google account on the device, that accepts all auth tokens.
     */
    public static Account setupTestAccountThatAcceptsAllAuthTokens(
            MockAccountManager accountManager,
            String accountName, String password) {
        Account account = AccountManagerHelper.createAccountFromName(accountName);
        AccountHolder.Builder accountHolder =
                AccountHolder.create().account(account).password(password).alwaysAccept(true);
        accountManager.addAccountHolderExplicitly(accountHolder.build());
        return account;
    }

    /**
     * Returns whether the sync engine has keep everything synced set to true.
     */
    public static boolean isSyncEverythingEnabled(final Context context) {
        final AtomicBoolean result = new AtomicBoolean();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                result.set(ProfileSyncService.get(context).hasKeepEverythingSynced());
            }
        });
        return result.get();
    }

    /**
     * Verifies that the sync status is "Syncing not enabled" and that sync is signed in with the
     * account.
     */
    public static void verifySyncIsDisabled(Context context, Account account) {
        Map<Pair<String, String>, String> expectedStats =
                new HashMap<Pair<String, String>, String>();
        expectedStats.put(SYNC_SUMMARY_STATUS, UNINITIALIZED);
        Assert.assertTrue(
                "Expected sync to be disabled.", pollAboutSyncStats(context, expectedStats));
        verifySignedInWithAccount(context, account);
    }

    /**
     * Retrieves the sync internals information which is the basis for chrome://sync-internals and
     * makes the result available in {@link AboutSyncInfoGetter#getAboutInfo()}.
     *
     * This class has to be run on the main thread, as it accesses the ProfileSyncService.
     */
    public static class AboutSyncInfoGetter implements Runnable {
        private static final String TAG = "AboutSyncInfoGetter";
        final Context mContext;
        Map<Pair<String, String>, String> mAboutInfo;

        public AboutSyncInfoGetter(Context context) {
            mContext = context.getApplicationContext();
            mAboutInfo = new HashMap<Pair<String, String>, String>();
        }

        @Override
        public void run() {
            String info = ProfileSyncService.get(mContext).getSyncInternalsInfoForTest();
            try {
                mAboutInfo = getAboutInfoStats(info);
            } catch (JSONException e) {
                Log.w(TAG, "Unable to parse JSON message: " + info, e);
            }
        }

        public Map<Pair<String, String>, String> getAboutInfo() {
            return mAboutInfo;
        }
    }

    /**
     * Helper class used to create a mock account on the device.
     */
    public static class SyncTestContext extends AdvancedMockContext {

        public SyncTestContext(Context context) {
            super(context);
        }

        @Override
        public Object getSystemService(String name) {
            if (Context.ACCOUNT_SERVICE.equals(name)) {
                throw new UnsupportedOperationException(
                        "Sync tests should not use system Account Manager.");
            }
            return super.getSystemService(name);
        }
    }
}

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
import org.chromium.chrome.browser.invalidation.InvalidationServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.sync.signin.ChromeSigninController;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Utility class for shared sync test functionality.
 */
public final class SyncTestUtil {
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
                InvalidationServiceFactory.getForProfile(Profile.getLastUsedProfile())
                        .requestSyncFromNativeChromeForAllTypes();
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
                result.set(ProfileSyncService.get().getLastSyncedTimeForTest());
                s.release();
            }
        });
        Assert.assertTrue(s.tryAcquire(SYNC_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        return result.get();
    }

    /**
     * Waits for sync to become active.
     */
    public static void waitForSyncActive(final Context context) throws InterruptedException {
        Assert.assertTrue("Timed out waiting for sync to become active.",
                CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return ProfileSyncService.get().isSyncActive();
                    }
                }, SYNC_WAIT_TIMEOUT_MS, SYNC_CHECK_INTERVAL_MS));
    }

    /**
     * Verifies that the sync is active and signed in with the given account.
     */
    public static void verifySyncIsActiveForAccount(Context context, Account account)
            throws InterruptedException {
        waitForSyncActive(context);
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
     * Returns whether the sync engine has keep everything synced set to true.
     */
    public static boolean isSyncEverythingEnabled(final Context context) {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return ProfileSyncService.get().hasKeepEverythingSynced();
            }
        });
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
     * Retrieves the local Sync data as a JSONArray via ProfileSyncService.
     *
     * This method blocks until the data is available or until it times out.
     */
    private static JSONArray getAllNodesAsJsonArray(final Context context) throws JSONException {
        final Semaphore semaphore = new Semaphore(0);
        final ProfileSyncService.GetAllNodesCallback callback =
                new ProfileSyncService.GetAllNodesCallback() {
                    @Override
                    public void onResult(String nodesString) {
                        super.onResult(nodesString);
                        semaphore.release();
                    }
        };

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ProfileSyncService.get().getAllNodes(callback);
            }
        });

        try {
            Assert.assertTrue("Semaphore should have been released.",
                    semaphore.tryAcquire(UI_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }

        return callback.getNodesAsJsonArray();
    }


    /**
     * Extracts datatype-specific information from the given JSONObject. The returned JSONObject
     * contains the same data as a specifics protocol buffer (e.g., TypedUrlSpecifics).
     */
    private static JSONObject extractSpecifics(JSONObject node) throws JSONException {
        JSONObject specifics = node.getJSONObject("SPECIFICS");
        // The key name here is type-specific (e.g., "typed_url" for Typed URLs), so we
        // can't hard code a value.
        Iterator<String> keysIterator = specifics.keys();
        String key = null;
        if (!keysIterator.hasNext()) {
            throw new JSONException("Specifics object has 0 keys.");
        }
        key = keysIterator.next();

        if (keysIterator.hasNext()) {
            throw new JSONException("Specifics object has more than 1 key.");
        }

        if (key.equals("bookmark")) {
            JSONObject bookmarkSpecifics = specifics.getJSONObject(key);
            bookmarkSpecifics.put("parent_id", node.getString("PARENT_ID"));
            return bookmarkSpecifics;
        }
        return specifics.getJSONObject(key);
    }

    /**
     * Converts the given ID to the format stored by the server.
     *
     * See the SyncableId (C++) class for more information about ID encoding. To paraphrase,
     * the client prepends "s" or "c" to the server's ID depending on the commit state of the data.
     * IDs can also be "r" to indicate the root node, but that entity is not supported here.
     *
     * @param clientId the ID to be converted
     * @return the converted ID
     */
    private static String convertToServerId(String clientId) {
        if (clientId == null) {
            throw new IllegalArgumentException("Client entity ID cannot be null.");
        } else if (clientId.isEmpty()) {
            throw new IllegalArgumentException("Client ID cannot be empty.");
        } else if (!clientId.startsWith("s") && !clientId.startsWith("c")) {
            throw new IllegalArgumentException(String.format(
                    "Client ID (%s) must start with c or s.", clientId));
        }

        return clientId.substring(1);
    }

    /**
     * Returns the local Sync data present for a single datatype.
     *
     * For each data entity, a Pair is returned. The first piece of data is the entity's server ID.
     * This is useful for activities like deleting an entity on the server. The second piece of data
     * is a JSONObject representing the datatype-specific information for the entity. This data is
     * the same as the data stored in a specifics protocol buffer (e.g., TypedUrlSpecifics).
     *
     * @param context the Context used to retreive the correct ProfileSyncService
     * @param typeString a String representing a specific datatype.
     *
     * TODO(pvalenzuela): Replace typeString with the native ModelType enum or something else
     * that will avoid callers needing to specify the native string version.
     *
     * @return a List of Pair<String, JSONObject> representing the local Sync data
     */
    public static List<Pair<String, JSONObject>> getLocalData(
            Context context, String typeString) throws JSONException {
        JSONArray localData = getAllNodesAsJsonArray(context);
        JSONArray datatypeNodes = new JSONArray();
        for (int i = 0; i < localData.length(); i++) {
            JSONObject datatypeObject = localData.getJSONObject(i);
            if (datatypeObject.getString("type").equals(typeString)) {
                datatypeNodes = datatypeObject.getJSONArray("nodes");
                break;
            }
        }

        List<Pair<String, JSONObject>> localDataForDatatype =
                new ArrayList<Pair<String, JSONObject>>(datatypeNodes.length());
        for (int i = 0; i < datatypeNodes.length(); i++) {
            JSONObject entity = datatypeNodes.getJSONObject(i);
            if (!entity.getString("UNIQUE_SERVER_TAG").isEmpty()) {
                // Ignore permanent items (e.g., root datatype folders).
                continue;
            }
            String id = convertToServerId(entity.getString("ID"));
            localDataForDatatype.add(Pair.create(id, extractSpecifics(entity)));
        }
        return localDataForDatatype;
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
            String info = ProfileSyncService.get().getSyncInternalsInfoForTest();
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
}

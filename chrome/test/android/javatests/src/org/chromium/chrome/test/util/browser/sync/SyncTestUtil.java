// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.sync;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.accounts.Account;
import android.content.Context;
import android.util.Pair;

import junit.framework.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.invalidation.InvalidationServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.sync.signin.ChromeSigninController;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/**
 * Utility class for shared sync test functionality.
 */
public final class SyncTestUtil {
    private static final String TAG = "SyncTestUtil";

    public static final long TIMEOUT_MS = scaleTimeout(20000);
    public static final int INTERVAL_MS = 250;

    private SyncTestUtil() {}

    /**
     * Verifies that sync is signed out.
     */
    public static void verifySyncIsSignedOut(Context context) {
        Assert.assertTrue(isSyncOff());
    }

    /**
     * Triggers a sync cycle.
     */
    public static void triggerSync() throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                InvalidationServiceFactory.getForProfile(Profile.getLastUsedProfile())
                        .requestSyncFromNativeChromeForAllTypes();
            }
        });
    }

    /**
     * Triggers a sync and waits till it is complete.
     */
    public static void triggerSyncAndWaitForCompletion() throws InterruptedException {
        final long oldSyncTime = getCurrentSyncTime();
        triggerSync();
        CriteriaHelper.pollForCriteria(new Criteria(
                "Timed out waiting for sync cycle to complete.") {
            @Override
            public boolean isSatisfied() {
                return getCurrentSyncTime() > oldSyncTime;
            }
        }, TIMEOUT_MS, INTERVAL_MS);
    }

    private static long getCurrentSyncTime() {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Long>() {
            @Override
            public Long call() {
                return ProfileSyncService.get().getLastSyncedTimeForTest();
            }
        });
    }

    /**
     * Waits for sync to become active.
     */
    public static void waitForSyncActive() throws InterruptedException {
        CriteriaHelper.pollForUIThreadCriteria(new Criteria(
                "Timed out waiting for sync to become active.") {
            @Override
            public boolean isSatisfied() {
                return ProfileSyncService.get().isSyncActive();
            }
        }, TIMEOUT_MS, INTERVAL_MS);
    }

    /**
     * Verifies that the sync is active and signed in with the given account.
     */
    public static void verifySyncIsActiveForAccount(Context context, Account account)
            throws InterruptedException {
        waitForSyncActive();
        triggerSyncAndWaitForCompletion();
        verifySignedInWithAccount(context, account);
    }

    /**
     * Makes sure that sync is enabled with the correct account.
     */
    public static void verifySignedInWithAccount(Context context, Account account) {
        Assert.assertEquals(account, ChromeSigninController.get(context).getSignedInUser());
    }

    /**
     * Verifies that the sync is off but signed in with the account.
     */
    public static void verifySyncIsDisabled(Context context, Account account) {
        Assert.assertTrue("Expected sync to be disabled.", isSyncOff());
        verifySignedInWithAccount(context, account);
    }

    private static boolean isSyncOff() {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                ProfileSyncService syncService = ProfileSyncService.get();
                return !syncService.isBackendInitialized() && !syncService.isSyncRequested();
            }
        });
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
                    semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
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
}

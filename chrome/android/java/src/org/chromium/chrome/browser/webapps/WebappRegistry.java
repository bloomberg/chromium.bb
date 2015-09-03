// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.AsyncTask;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * Keeps track of webapps which have created a SharedPreference file (through the used of the
 * WebappDataStorage class) which may need to be cleaned up in the future.
 *
 * It is NOT intended to be 100% accurate nor a comprehensive list of all installed web apps
 * because it is impossible to track when the user removes a web app from the Home screen and it
 * is similarily impossible to track pre-registry era web apps (this case is not a problem anyway
 * as these web apps have no external data to cleanup).
 */
public class WebappRegistry {

    static final String REGISTRY_FILE_NAME = "webapp_registry";
    static final String KEY_WEBAPP_SET = "webapp_set";

    /**
     * Registers the existence of a web app and creates the SharedPreference for it.
     * @param context  Context to open the registry with.
     * @param webappId The id of the web app to register.
     */
    public static void registerWebapp(final Context context, final String webappId) {
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected final Void doInBackground(Void... nothing) {
                SharedPreferences preferences = context.getSharedPreferences(
                        REGISTRY_FILE_NAME, Context.MODE_PRIVATE);
                Set<String> webapps = new HashSet<String>(
                        preferences.getStringSet(KEY_WEBAPP_SET, Collections.<String>emptySet()));
                boolean added = webapps.add(webappId);
                assert added;

                // Update the last used time of the webapp data storage so we can guarantee that
                // the used time will be set (ie. != WebappDataStorage.INVALID_LAST_USED) if a
                // webapp appears in the registry.
                new WebappDataStorage(context, webappId).updateLastUsedTime();
                preferences.edit().putStringSet(KEY_WEBAPP_SET, webapps).commit();
                return null;
            }
        }.execute();
    }

    /**
     * Asynchronously retrieves the list of web app IDs which this registry is aware of.
     * @param context  Context to open the registry with.
     * @param callback Called when the set has been retrieved. The set may be empty.
     */
    public static void getRegisteredWebappIds(final Context context, final FetchCallback callback) {
        new AsyncTask<Void, Void, Set<String>>() {
            @Override
            protected final Set<String> doInBackground(Void... nothing) {
                SharedPreferences preferences = context.getSharedPreferences(
                        REGISTRY_FILE_NAME, Context.MODE_PRIVATE);
                return preferences.getStringSet(KEY_WEBAPP_SET, Collections.<String>emptySet());
            }

            @Override
            protected final void onPostExecute(Set<String> result) {
                callback.onWebappIdsRetrieved(result);
            }
        }.execute();
    }

    private WebappRegistry() {
    }

    /**
     * Called when a retrieval of the stored web apps occurs.
     */
    public interface FetchCallback {
        public void onWebappIdsRetrieved(Set<String> readObject);
    }
}
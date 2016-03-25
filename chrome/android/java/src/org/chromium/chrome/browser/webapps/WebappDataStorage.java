// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.os.AsyncTask;

import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ShortcutHelper;

import java.util.Map;

/**
 * Stores data about an installed web app. Uses SharedPreferences to persist the data to disk.
 * Before this class is used, the web app must be registered in {@link WebappRegistry}.
 *
 * EXAMPLE USAGE:
 *
 * (1) UPDATING/RETRIEVING THE ICON (web app MUST have been registered in WebappRegistry)
 * WebappDataStorage storage = WebappDataStorage.open(context, id);
 * storage.updateSplashScreenImage(bitmap);
 * storage.getSplashScreenImage(callback);
 */
public class WebappDataStorage {

    static final String SHARED_PREFS_FILE_PREFIX = "webapp_";
    static final String KEY_SPLASH_ICON = "splash_icon";
    static final String KEY_LAST_USED = "last_used";
    static final String KEY_SCOPE = "scope";

    // Unset/invalid constants for last used times and scopes. 0 is used as the null last
    // used time as WebappRegistry assumes that this is always a valid timestamp.
    static final long LAST_USED_UNSET = 0;
    static final long LAST_USED_INVALID = -1;
    static final String SCOPE_INVALID = "";

    private static Factory sFactory = new Factory();

    private final SharedPreferences mPreferences;

    /**
     * Opens an instance of WebappDataStorage for the web app specified.
     * @param context  The context to open the SharedPreferences.
     * @param webappId The ID of the web app which is being opened.
     */
    public static WebappDataStorage open(final Context context, final String webappId) {
        final WebappDataStorage storage = sFactory.create(context, webappId);
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected final Void doInBackground(Void... nothing) {
                if (storage.getLastUsedTime() == LAST_USED_INVALID) {
                    // If the last used time is invalid then assert that there is no data
                    // in the WebappDataStorage which needs to be cleaned up.
                    assert storage.getAllData().isEmpty();
                } else {
                    storage.updateLastUsedTime();
                }
                return null;
            }
        }.execute();
        return storage;
    }

    /**
     * Asynchronously retrieves the time which this WebappDataStorage was last
     * opened using {@link WebappDataStorage#open(Context, String)}.
     * @param context  The context to read the SharedPreferences file.
     * @param webappId The ID of the web app the used time is being read for.
     * @param callback Called when the last used time has been retrieved.
     */
    @VisibleForTesting
    public static void getLastUsedTime(final Context context, final String webappId,
            final FetchCallback<Long> callback) {
        new AsyncTask<Void, Void, Long>() {
            @Override
            protected final Long doInBackground(Void... nothing) {
                long lastUsed = new WebappDataStorage(context.getApplicationContext(), webappId)
                        .getLastUsedTime();
                assert lastUsed != LAST_USED_INVALID;
                return lastUsed;
            }

            @Override
            protected final void onPostExecute(Long lastUsed) {
                callback.onDataRetrieved(lastUsed);
            }
        }.execute();
    }

    /**
     * Asynchronously retrieves the scope stored in this WebappDataStorage. The scope is the URL
     * over which the webapp data is applied to.
     * @param context  The context to read the SharedPreferences file.
     * @param webappId The ID of the web app the used time is being read for.
     * @param callback Called when the scope has been retrieved.
     */
    @VisibleForTesting
    public static void getScope(final Context context, final String webappId,
            final FetchCallback<String> callback) {
        new AsyncTask<Void, Void, String>() {
            @Override
            protected final String doInBackground(Void... nothing) {
                return new WebappDataStorage(context.getApplicationContext(), webappId)
                        .getScope();
            }

            @Override
            protected final void onPostExecute(String scope) {
                callback.onDataRetrieved(scope);
            }
        }.execute();
    }

    /**
     * Asynchronously sets the scope stored in this WebappDataStorage. Does nothing if there
     * is already a scope stored; since webapps added to homescreen cannot change the scope which
     * they launch, it is not intended that a WebappDataStorage will be able to change the scope
     * once it is set.
     * @param context  The context to read the SharedPreferences file.
     * @param webappId The ID of the web app the used time is being read for.
     * @param scope The scope to set for the web app.
     */
    public static void setScope(final Context context, final String webappId, final String scope) {
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected final Void doInBackground(Void... nothing) {
                new WebappDataStorage(context.getApplicationContext(), webappId).setScope(scope);
                return null;
            }
        }.execute();
    }

    /**
     * Deletes the data for a web app by clearing all the information inside the SharedPreferences
     * file. This does NOT delete the file itself but the file is left empty.
     * @param context  The context to read the SharedPreferences file.
     * @param webappId The ID of the web app being deleted.
     */
    static void deleteDataForWebapp(final Context context, final String webappId) {
        assert !ThreadUtils.runningOnUiThread();
        openSharedPreferences(context, webappId).edit().clear().apply();
    }

    /**
     * Deletes the scope and sets last used time to 0 this web app in SharedPreferences.
     * This does not remove the stored splash screen image (if any) for the app.
     * @param context  The context to read the SharedPreferences file.
     * @param webappId The ID of the web app being deleted.
     */
    static void clearHistory(final Context context, final String webappId) {
        // The last used time is set to 0 to ensure that a valid value is always present.
        // If the webapp is not launched prior to the next cleanup, then its remaining data will be
        // removed. Otherwise, the next launch will update the last used time.
        assert !ThreadUtils.runningOnUiThread();
        openSharedPreferences(context, webappId)
                .edit().putLong(KEY_LAST_USED, LAST_USED_UNSET).remove(KEY_SCOPE).apply();
    }

    /**
     * Sets the factory used to generate WebappDataStorage objects.
     */
    @VisibleForTesting
    public static void setFactoryForTests(Factory factory) {
        sFactory = factory;
    }

    private static SharedPreferences openSharedPreferences(Context context, String webappId) {
        return context.getApplicationContext().getSharedPreferences(
                SHARED_PREFS_FILE_PREFIX + webappId, Context.MODE_PRIVATE);
    }

    protected WebappDataStorage(Context context, String webappId) {
        mPreferences = openSharedPreferences(context, webappId);
    }

    /*
     * Asynchronously retrieves the splash screen image associated with the
     * current web app.
     * @param callback Called when the splash screen image has been retrieved.
     *                 May be null if no image was found.
     */
    public void getSplashScreenImage(final FetchCallback<Bitmap> callback) {
        new AsyncTask<Void, Void, Bitmap>() {
            @Override
            protected final Bitmap doInBackground(Void... nothing) {
                return ShortcutHelper.decodeBitmapFromString(
                        mPreferences.getString(KEY_SPLASH_ICON, null));
            }

            @Override
            protected final void onPostExecute(Bitmap result) {
                callback.onDataRetrieved(result);
            }
        }.execute();
    }

    /*
     * Update the information associated with the web app with the specified data.
     * @param splashScreenImage The image which should be shown on the splash screen of the web app.
     */
    public void updateSplashScreenImage(final Bitmap splashScreenImage) {
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected final Void doInBackground(Void... nothing) {
                mPreferences.edit()
                        .putString(KEY_SPLASH_ICON,
                                ShortcutHelper.encodeBitmapAsString(splashScreenImage))
                        .apply();
                return null;
            }
        }.execute();
    }

    /**
     * Updates the scope stored in this object. Does nothing if there is already a scope stored.
     * @param scope the scope to store.
     */
    void setScope(String scope) {
        assert !ThreadUtils.runningOnUiThread();
        if (mPreferences.getString(KEY_SCOPE, SCOPE_INVALID).equals(SCOPE_INVALID)) {
            mPreferences.edit().putString(KEY_SCOPE, scope).apply();
        }
    }

    /**
     * Returns the scope stored in this object, or "" if it is not stored.
     */
    String getScope() {
        assert !ThreadUtils.runningOnUiThread();
        return mPreferences.getString(KEY_SCOPE, SCOPE_INVALID);
    }

    /**
     * Updates the last used time of this object.
     * @param lastUsedTime the new last used time.
     */
    void updateLastUsedTime() {
        assert !ThreadUtils.runningOnUiThread();
        mPreferences.edit().putLong(KEY_LAST_USED, System.currentTimeMillis()).apply();
    }

    /**
     * Returns the last used time of this object, or -1 if it is not stored.
     */
    long getLastUsedTime() {
        assert !ThreadUtils.runningOnUiThread();
        return mPreferences.getLong(KEY_LAST_USED, LAST_USED_INVALID);
    }

    private Map<String, ?> getAllData() {
        return mPreferences.getAll();
    }

    /**
     * Called after data has been retrieved from storage.
     */
    public interface FetchCallback<T> {
        public void onDataRetrieved(T readObject);
    }

    /**
     * Factory used to generate WebappDataStorage objects.
     *
     * It is used in tests to override methods in WebappDataStorage and inject the mocked objects.
     */
    public static class Factory {

        /**
         * Generates a WebappDataStorage class for a specified web app.
         */
        public WebappDataStorage create(final Context context, final String webappId) {
            return new WebappDataStorage(context, webappId);
        }
    }
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteException;
import android.util.Log;
import android.webkit.WebView;

/**
 * This database is used to support
 * {@link WebView#setHttpAuthUsernamePassword(String, String, String, String)} and
 * {@link WebView#getHttpAuthUsernamePassword(String, String)}.
 *
 * This class is a singleton and should be accessed only via
 * {@link HttpAuthDatabase#getInstance(Context)}.  Calling this method will create an instance on
 * demand and start the process of opening or creating the database in the background.
 *
 * Unfortunately, to line up with the WebViewClassic API and behavior, there isn't much we can do
 * when SQL errors occur.
 */
public class HttpAuthDatabase {

    private static final String DATABASE_FILE = "http_auth.db";

    private static final String LOGTAG = HttpAuthDatabase.class.getName();

    private static final int DATABASE_VERSION = 1;

    private static HttpAuthDatabase sInstance = null;

    private static SQLiteDatabase sDatabase = null;

    private static final String ID_COL = "_id";

    private static final String[] ID_PROJECTION = new String[] {
        ID_COL
    };

    // column id strings for "httpauth" table
    private static final String HTTPAUTH_TABLE_NAME = "httpauth";
    private static final String HTTPAUTH_HOST_COL = "host";
    private static final String HTTPAUTH_REALM_COL = "realm";
    private static final String HTTPAUTH_USERNAME_COL = "username";
    private static final String HTTPAUTH_PASSWORD_COL = "password";

    /**
     * Initially false until the background thread completes.
     */
    private boolean mInitialized = false;

    private HttpAuthDatabase(final Context context) {
        // Singleton only, use getInstance()
        new Thread() {
            @Override
            public void run() {
                init(context);
            }
        }.start();
    }

    /**
     * Fetches the singleton instance of HttpAuthDatabase, creating an instance on demand.
     *
     * @param context the Context to use for opening the database
     * @return The singleton instance
     */
    public static synchronized HttpAuthDatabase getInstance(Context context) {
        if (sInstance == null) {
            sInstance = new HttpAuthDatabase(context);
        }
        return sInstance;
    }

    /**
     * Initializes the databases and notifies any callers waiting on waitForInit.
     *
     * @param context the Context to use for opening the database
     */
    private synchronized void init(Context context) {
        if (mInitialized) {
            return;
        }

        initDatabase(context);

        // Thread done, notify.
        mInitialized = true;
        notify();
    }

    /**
     * Opens the database, and upgrades it if necessary.
     *
     * @param context the Context to use for opening the database
     */
    private static void initDatabase(Context context) {
        try {
            sDatabase = context.openOrCreateDatabase(DATABASE_FILE, 0, null);
        } catch (SQLiteException e) {
            // try again by deleting the old db and create a new one
            if (context.deleteDatabase(DATABASE_FILE)) {
                sDatabase = context.openOrCreateDatabase(DATABASE_FILE, 0, null);
            }
        }

        if (sDatabase == null) {
            // Not much we can do to recover at this point
            Log.e(LOGTAG, "Unable to open or create " + DATABASE_FILE);
        }

        if (sDatabase.getVersion() != DATABASE_VERSION) {
            sDatabase.beginTransactionNonExclusive();
            try {
                createTable();
                sDatabase.setTransactionSuccessful();
            } finally {
                sDatabase.endTransaction();
            }
        }
    }

    private static void createTable() {
        sDatabase.execSQL("CREATE TABLE " + HTTPAUTH_TABLE_NAME
                + " (" + ID_COL + " INTEGER PRIMARY KEY, "
                + HTTPAUTH_HOST_COL + " TEXT, " + HTTPAUTH_REALM_COL
                + " TEXT, " + HTTPAUTH_USERNAME_COL + " TEXT, "
                + HTTPAUTH_PASSWORD_COL + " TEXT," + " UNIQUE ("
                + HTTPAUTH_HOST_COL + ", " + HTTPAUTH_REALM_COL
                + ") ON CONFLICT REPLACE);");

        sDatabase.setVersion(DATABASE_VERSION);
    }

    /**
     * Waits for the background initialization thread to complete and check the database creation
     * status.
     *
     * @return true if the database was initialized, false otherwise
     */
    private boolean waitForInit() {
        synchronized (this) {
            while (!mInitialized) {
                try {
                    wait();
                } catch (InterruptedException e) {
                    Log.e(LOGTAG, "Caught exception while checking initialization", e);
                }
            }
        }
        return sDatabase != null;
    }

    /**
     * Sets the HTTP authentication password. Tuple (HTTPAUTH_HOST_COL, HTTPAUTH_REALM_COL,
     * HTTPAUTH_USERNAME_COL) is unique.
     *
     * @param host the host for the password
     * @param realm the realm for the password
     * @param username the username for the password.
     * @param password the password
     */
    // TODO(leandrogracia): remove public when @VisibleForTesting works.
    // @VisibleForTesting
    public void setHttpAuthUsernamePassword(String host, String realm, String username,
            String password) {
        if (host == null || realm == null || !waitForInit()) {
            return;
        }

        final ContentValues c = new ContentValues();
        c.put(HTTPAUTH_HOST_COL, host);
        c.put(HTTPAUTH_REALM_COL, realm);
        c.put(HTTPAUTH_USERNAME_COL, username);
        c.put(HTTPAUTH_PASSWORD_COL, password);
        sDatabase.insert(HTTPAUTH_TABLE_NAME, HTTPAUTH_HOST_COL, c);
    }

    /**
     * Retrieves the HTTP authentication username and password for a given host and realm pair. If
     * there are multiple username/password combinations for a host/realm, only the first one will
     * be returned.
     *
     * @param host the host the password applies to
     * @param realm the realm the password applies to
     * @return a String[] if found where String[0] is username (which can be null) and
     *         String[1] is password.  Null is returned if it can't find anything.
     */
    // TODO(leandrogracia): remove public when @VisibleForTesting works.
    // @VisibleForTesting
    public String[] getHttpAuthUsernamePassword(String host, String realm) {
        if (host == null || realm == null || !waitForInit()){
            return null;
        }

        final String[] columns = new String[] {
            HTTPAUTH_USERNAME_COL, HTTPAUTH_PASSWORD_COL
        };
        final String selection = "(" + HTTPAUTH_HOST_COL + " == ?) AND " +
                "(" + HTTPAUTH_REALM_COL + " == ?)";

        String[] ret = null;
        Cursor cursor = null;
        try {
            cursor = sDatabase.query(HTTPAUTH_TABLE_NAME, columns, selection,
                    new String[] { host, realm }, null, null, null);
            if (cursor.moveToFirst()) {
                ret = new String[] {
                        cursor.getString(cursor.getColumnIndex(HTTPAUTH_USERNAME_COL)),
                        cursor.getString(cursor.getColumnIndex(HTTPAUTH_PASSWORD_COL)),
                };
            }
        } catch (IllegalStateException e) {
            Log.e(LOGTAG, "getHttpAuthUsernamePassword", e);
        } finally {
            if (cursor != null) cursor.close();
        }
        return ret;
    }

    /**
     * Determines if there are any HTTP authentication passwords saved.
     *
     * @return true if there are passwords saved
     */
    public boolean hasHttpAuthUsernamePassword() {
        if (!waitForInit()) {
            return false;
        }

        Cursor cursor = null;
        boolean ret = false;
        try {
            cursor = sDatabase.query(HTTPAUTH_TABLE_NAME, ID_PROJECTION, null, null, null, null,
                    null);
            ret = cursor.moveToFirst();
        } catch (IllegalStateException e) {
            Log.e(LOGTAG, "hasEntries", e);
        } finally {
            if (cursor != null) cursor.close();
        }
        return ret;
    }

    /**
     * Clears the HTTP authentication password database.
     */
    public void clearHttpAuthUsernamePassword() {
        if (!waitForInit()) {
            return;
        }
        sDatabase.delete(HTTPAUTH_TABLE_NAME, null, null);
    }
}

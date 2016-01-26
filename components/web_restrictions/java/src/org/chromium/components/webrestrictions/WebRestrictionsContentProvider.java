// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webrestrictions;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.content.UriMatcher;
import android.content.pm.ProviderInfo;
import android.database.AbstractCursor;
import android.database.Cursor;
import android.net.Uri;
import android.util.Pair;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Abstract content provider for providing web restrictions, i.e. for providing a filter for URLs so
 * that they can be blocked or permitted, and a means of requesting permission for new URLs. It
 * provides two (virtual) tables; an 'authorized' table listing the the status of every URL, and a
 * 'requested' table containing the requests for access to new URLs. The 'authorized' table is read
 * only and the 'requested' table is write only.
 */
public abstract class WebRestrictionsContentProvider extends ContentProvider {
    public static final int BLOCKED = 0;
    public static final int PROCEED = 1;
    private static final int WEB_RESTRICTIONS = 1;
    private static final int AUTHORIZED = 2;
    private static final int REQUESTED = 3;

    private final Pattern mSelectionPattern;
    private UriMatcher mContentUriMatcher;
    private Uri mContentUri;

    protected WebRestrictionsContentProvider() {
        // Pattern to extract the URL from the selection.
        // Matches patterns of the form "url = '<url>'" with arbitrary spacing around the "=" etc.
        mSelectionPattern = Pattern.compile("\\s*url\\s*=\\s*'([^']*)'");
    }

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public void attachInfo(Context context, ProviderInfo info) {
        super.attachInfo(context, info);
        mContentUri = new Uri.Builder().scheme("content").authority(info.authority).build();
        mContentUriMatcher = new UriMatcher(WEB_RESTRICTIONS);
        mContentUriMatcher.addURI(info.authority, "authorized", AUTHORIZED);
        mContentUriMatcher.addURI(info.authority, "requested", REQUESTED);
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs,
            String sortOrder) {
        if (!contentProviderEnabled()) return null;
        // Check that this is the a query on the 'authorized' table
        // TODO(aberent): Provide useful queries on the 'requested' table.
        if (mContentUriMatcher.match(uri) != AUTHORIZED) return null;
        // If the selection is of the right form get the url we are querying.
        Matcher matcher = mSelectionPattern.matcher(selection);
        if (!matcher.find()) return null;
        final String url = matcher.group(1);
        final Pair<Boolean, String> result = shouldProceed(url);

        return new AbstractCursor() {

            @Override
            public int getCount() {
                return 1;
            }

            @Override
            public String[] getColumnNames() {
                return new String[] {
                        "Should Proceed", "Error message"
                };
            }

            @Override
            public String getString(int column) {
                if (column == 1) return result.second;
                return null;
            }

            @Override
            public short getShort(int column) {
                return 0;
            }

            @Override
            public int getInt(int column) {
                if (column == 0) return result.first ? PROCEED : BLOCKED;
                return 0;
            }

            @Override
            public long getLong(int column) {
                return 0;
            }

            @Override
            public float getFloat(int column) {
                return 0;
            }

            @Override
            public double getDouble(int column) {
                return 0;
            }

            @Override
            public boolean isNull(int column) {
                return false;
            }
        };
    }

    @Override
    public String getType(Uri uri) {
        // Abused to return whether we can insert
        if (!contentProviderEnabled()) return null;
        if (mContentUriMatcher.match(uri) != REQUESTED) return null;
        return canInsert() ? "text/plain" : null;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        if (!contentProviderEnabled()) return null;
        if (mContentUriMatcher.match(uri) != REQUESTED) return null;
        String url = values.getAsString("url");
        if (requestInsert(url)) {
            // TODO(aberent): If we ever make the 'requested' table readable then we might want to
            // change this to a more conventional content URI (with a row number).
            return uri.buildUpon().appendPath(url).build();
        } else {
            return null;
        }
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        return 0;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        return 0;
    }

    /**
     * @param url the URL that is wanted.
     * @return a pair containing the Result and the HTML Error Message. result is true if safe to
     *         proceed, false otherwise. error message is only meaningful if result is false, a null
     *         error message means use application default.
     */
    protected abstract Pair<Boolean, String> shouldProceed(final String url);

    /**
     * @return whether the content provider allows insertions.
     */
    protected abstract boolean canInsert();

    /**
     * Start a request that a URL should be permitted
     *
     * @param url the URL that is wanted.
     */
    protected abstract boolean requestInsert(final String url);

    /**
     * @return true if the content provider is enabled, false if not
     */
    protected abstract boolean contentProviderEnabled();

    /**
     * Call to tell observers that the filter has changed.
     */
    protected void onFilterChanged() {
        getContext().getContentResolver().notifyChange(
                mContentUri.buildUpon().appendPath("authorized").build(), null);
    }
}

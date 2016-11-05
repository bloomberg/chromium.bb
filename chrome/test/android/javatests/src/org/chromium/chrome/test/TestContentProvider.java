// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Content provider for testing content URLs.
 */

package org.chromium.chrome.test;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.database.AbstractCursor;
import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;

import org.chromium.base.Log;
import org.chromium.base.test.util.UrlUtils;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.net.URLConnection;
import java.util.HashMap;
import java.util.Map;

/**
 * Content provider for testing content:// urls.
 * Note: if you move this class, make sure you have also updated AndroidManifest.xml
 */
public class TestContentProvider extends ContentProvider {
    private static final String ANDROID_DATA_FILE_PATH = "android/";
    private static final String AUTHORITY = "org.chromium.chrome.test.TestContentProvider";
    private static final String CONTENT_SCHEME = "content://";
    private static final String GET_RESOURCE_REQUEST_COUNT = "get_resource_request_count";
    private static final String RESET_RESOURCE_REQUEST_COUNTS = "reset_resource_request_counts";
    private static final String TAG = "TestContentProvider";
    private enum ColumnIndex {
        RESOURCE_REQUEST_COUNT_COLUMN,
    }
    private Map<String, Integer> mResourceRequestCount;

    // Content providers can be accessed from multiple threads.
    private Object mLock = new Object();

    public static String createContentUrl(String target) {
        return CONTENT_SCHEME + AUTHORITY + "/" + target;
    }

    private static Uri createRequestUri(final String target, String resource) {
        if (resource == null) {
            return Uri.parse(createContentUrl(target));
        } else {
            return Uri.parse(createContentUrl(target) + "?" + resource);
        }
    }

    public static int getResourceRequestCount(Context context, String resource) {
        Uri uri = createRequestUri(GET_RESOURCE_REQUEST_COUNT, resource);
        final Cursor cursor = context.getContentResolver().query(uri, null, null, null, null);
        try {
            cursor.moveToFirst();
            return cursor.getInt(ColumnIndex.RESOURCE_REQUEST_COUNT_COLUMN.ordinal());
        } finally {
            cursor.close();
        }
    }

    public static void resetResourceRequestCounts(Context context) {
        Uri uri = createRequestUri(RESET_RESOURCE_REQUEST_COUNTS, null);
        // A null cursor is returned for this request.
        context.getContentResolver().query(uri, null, null, null, null);
    }

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public ParcelFileDescriptor openFile(final Uri uri, String mode) throws FileNotFoundException {
        String resource = uri.getLastPathSegment();
        synchronized (mLock) {
            if (mResourceRequestCount.containsKey(resource)) {
                mResourceRequestCount.put(resource, mResourceRequestCount.get(resource) + 1);
            } else {
                mResourceRequestCount.put(resource, 1);
            }
        }
        return createAsset(resource);
    }

    private static ParcelFileDescriptor createAsset(String resource) {
        try {
            return ParcelFileDescriptor.open(
                    new File(UrlUtils.getTestFilePath(ANDROID_DATA_FILE_PATH + resource)),
                    ParcelFileDescriptor.MODE_READ_ONLY);
        } catch (IOException e) {
            Log.e(TAG, e.getMessage(), e);
        }
        return null;
    }

    @Override
    public String getType(Uri uri) {
        return URLConnection.guessContentTypeFromName(uri.getLastPathSegment());
    }

    @Override
    public int update(Uri uri, ContentValues values, String where,
                      String[] whereArgs) {
        return 0;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        return 0;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        return null;
    }

    /**
     * Cursor object for retrieving resource request counters.
     */
    private static class ProviderStateCursor extends AbstractCursor {
        private final int mResourceRequestCount;

        public ProviderStateCursor(int resourceRequestCount) {
            mResourceRequestCount = resourceRequestCount;
        }

        @Override
        public boolean isNull(int columnIndex) {
            return columnIndex != ColumnIndex.RESOURCE_REQUEST_COUNT_COLUMN.ordinal();
        }

        @Override
        public int getCount() {
            return 1;
        }

        @Override
        public int getType(int columnIndex) {
            if (columnIndex == ColumnIndex.RESOURCE_REQUEST_COUNT_COLUMN.ordinal()) {
                return Cursor.FIELD_TYPE_INTEGER;
            }
            return Cursor.FIELD_TYPE_NULL;
        }

        private void unsupported() {
            throw new UnsupportedOperationException();
        }

        @Override
        public double getDouble(int columnIndex) {
            unsupported();
            return 0.0;
        }

        @Override
        public float getFloat(int columnIndex) {
            unsupported();
            return 0.0f;
        }

        @Override
        public int getInt(int columnIndex) {
            if (columnIndex == ColumnIndex.RESOURCE_REQUEST_COUNT_COLUMN.ordinal()) {
                return mResourceRequestCount;
            }
            return -1;
        }

        @Override
        public short getShort(int columnIndex) {
            unsupported();
            return 0;
        }

        @Override
        public long getLong(int columnIndex) {
            return getInt(columnIndex);
        }

        @Override
        public String getString(int columnIndex) {
            unsupported();
            return null;
        }

        @Override
        public String[] getColumnNames() {
            return new String[] { GET_RESOURCE_REQUEST_COUNT };
        }
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection,
                        String[] selectionArgs, String sortOrder) {
        synchronized (mLock) {
            String action = uri.getLastPathSegment();
            String resource = uri.getQuery();
            if (GET_RESOURCE_REQUEST_COUNT.equals(action)) {
                return new ProviderStateCursor(mResourceRequestCount.containsKey(resource)
                        ? mResourceRequestCount.get(resource) : 0);
            } else if (RESET_RESOURCE_REQUEST_COUNTS.equals(action)) {
                mResourceRequestCount = new HashMap<String, Integer>();
            }
            return null;
        }
    }
}
